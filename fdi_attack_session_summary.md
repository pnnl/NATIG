# Session Summary: Compromised-Endpoint FDI Attack (DNP3, Modbus, MMS, GOOSE)

**Date:** 2026-07-20
**Branch:** `feature/fdi` (off `feature/iec61850-mms`)
**Status:** Complete and validated end-to-end in Docker for all four protocols. Not yet validated on Unity.

---

## What this branch adds

A new "FDI" (false data injection) threat, distinct from the existing MIM attack. Where MIM is
on-path interception (a separate attacker node tampers with traffic between master/client and
outstation/server), FDI is a **compromised endpoint**: the real outstation/server/publisher
fabricates its own analog reading before it's ever reported, simulating a compromised meter/RTU
rather than a man-in-the-middle. See `attack_model_reference.md` (written before implementation
started) for the full architecture survey and design rationale this was built from.

**Why this is a distinct threat worth having**, not just a MIM variant: it introduces **no
network-topology signature** — no extra node in path, no relay hop, no latency artifact. The
existing MIM attack (and GOOSE's rogue-publisher) are both, in different ways, detectable via
network-layer features. FDI is only catchable via physical-consistency / state-estimation-style
checks, which is a genuinely different point in the detectability space — likely the actual
research value of this addition for the dissertation's cross-protocol comparison.

## Implementation pattern (identical across all four protocols)

- New `FdiFlag`/`fdi_flag` and `FdiID`/`FDI_ID` ns-3 attributes, mirroring the existing
  `mitmFlag`/`MIM_ID` pattern exactly.
- New `apply_fdi(name, realValue)`: resolves `node_id`/`point_id`/`Value_attck`/`AttackChance`
  attributes (set once at topology build time — no runtime JSON re-read, unlike `handle_MIM`,
  which is a real perf win since the FDI hook sits on a much hotter path than packet arrival).
  Returns a fabricated value when the point matches and the chance roll fires.
- Hooked into each protocol's `store_points()` (where HELICS telemetry from GridLAB-D gets
  written), just before the real value is committed — so the corruption happens at the point of
  measurement, matching the literature's actual definition of FDI, not at time of reporting.
- `StartApplication()` schedules `set_attack(true/false)` directly from `AttackStartTime`/
  `AttackEndTime` when `fdi_flag` is set. No explicit reset step is needed (unlike `handle_MIM`'s
  `resetToRealValue`): once the window closes, the next real HELICS update simply overwrites the
  fabricated one, since GridLAB-D keeps pushing telemetry the whole simulation.
- Topology wiring (`includeFDI`-gated, guarded against configs written before this key existed)
  sets these attributes directly on the **real** outstation/server/publisher instance already
  installed by the per-microgrid loop — no new attacker node, unlike MIM's wiring.

**GOOSE is the one structural divergence**, and deliberately so: it has no in-path position and no
poll-response to intercept, so FDI runs on the real **publisher** (`fdi_flag`), not the
rogue-publisher role (`mitm_flag`) the existing MIM attack uses. A fabricated value flows through
GOOSE's normal publish path and naturally trips `datasetChangedSinceLastPublish()`'s deadband/burst
logic exactly like a real physical change would — no `stNum`-forging needed, since this is already
the authoritative publisher, not something competing with it.

## Commits (in order)

1. `7449a35` — DNP3: `FdiFlag`/`FdiID` attributes, `apply_fdi()`, `store_points()` hook,
   `includeFDI` topology wiring, example config in `3G-conf-123/grid.json`.
2. `33d7026` — Modbus: same pattern, adapted for Modbus's address-translated register file
   (`SetHoldingRegister`) instead of a name-keyed map.
3. `74f756b` — MMS: same pattern, closer to DNP3's shape (name-keyed `m_deviceConfig.analogValues`).
4. `8715f10` — GOOSE: same pattern, targets the real publisher instead of the rogue role (see
   above).
5. `6ae2298` — **Unrelated bug fix**, found during validation: DNP3's existing MIM attack crashes
   (`std::stof("")` → `std::invalid_argument`) the first time its attack window opens against live
   telemetry. `handle_MIM()` re-reads `AttackConf`'s JSON at packet-arrival time, keyed by the
   object's own `MIM_ID` attribute — but the topology file never set either attribute on the MIM
   node, so the lookup map stayed permanently empty. Fixed by setting both.
6. `9e97410` — Same fix, ported to Modbus and MMS (identical gap, copy-pasted from DNP3's file).
   GOOSE was never affected — its rogue-publisher wiring already set both attributes correctly.

## Validation

Ran all four production topologies end-to-end in the `natig-modbus-test` Docker container, against
the real `helics_broker` + `gridlabd` + `IEEE_123_Dynamic.glm`, using the live-validated
`mg1`/`mg2`/`mg3`/`substation` config (`3G-conf-123/grid.json`, copied into the container's
`integration/control/mg_run/` working directory). Target: `node_36$voltage_A.real` in `mg1`,
fabricated to `132000`, `attack_chance: 0.8`, window `Start: 1` / `End: 20` (full sim length).

**Result — identical across all four protocols:** 15,259 FDI firings, continuous from the
configured start through the last simulated instant (`t=19.998s`), with the real underlying value
visibly continuing to evolve underneath the fabricated one the whole time (confirming the
automatic-restoration design actually works, not just in theory).

### Environment issues found and worked around (container-local, not committed — see below)

None of these are code bugs; they were gaps in this specific Docker container's state, discovered
because DNP3's MIM attack (and this container's `mg_run` validation directory) had apparently never
been exercised end-to-end against live telemetry before:

1. **Missing jsoncpp include path** — this container's cached `./waf configure` flags expect
   `json/json.h` under `-I/usr/include`, but jsoncpp installs to `/usr/include/jsoncpp/json/` here.
   Fixed with `ln -s /usr/include/jsoncpp/json /usr/include/json`.
2. **Stale installed shared library** — the built executables' RPATH resolves
   `libns3.35-helics-optimized.so` from `/rd2c/lib/` (last updated July 15, before any of this
   branch's work), not the freshly built copy in `build/lib/`. `./waf build` alone never refreshes
   the installed copy; only `./waf install` does. Worked around by exporting
   `LD_LIBRARY_PATH=/rd2c/ns-3-dev/build/lib:$LD_LIBRARY_PATH` before running.
3. **Incomplete GridLAB-D model in `mg_run/`** — `IEEE_123_Dynamic.glm` `#include`s three files
   (`IEEE_123_Diesels.glm`, `IEEE_123_Inverters_Mixed.glm`, `IEEE_123_Recorders.glm`) that weren't
   present in that directory, and the Recorders file needs `gen/`, `inverter/`, `load/`, `oh/`,
   `pow/`, `switch/` output subdirectories to exist. Copied the missing files in from the parent
   `integration/control/` directory and created the subdirectories.

None of this is committed to the repo — it's all container/environment state. If this container is
ever rebuilt or a fresh one is used, these three fixes will need to be redone (or `./waf install`
run properly) before any topology can execute past startup.

## Open items for next session(s)

- **Unity validation not done.** Same caveat as the MMS/GOOSE work before it — see
  `feedback_ns3_build_environment_gotchas.md`.
- **Replay and slow-DDoS** are still queued, each as its own branch/chat per the earlier branching
  decision (implemented across all four protocols per threat, for cross-protocol comparability).
- The GOOSE rogue-publisher path's production-topology attack-window firing still hasn't been
  independently re-verified since the note in `iec61850_session_summary.md:164` — worth doing
  before building replay on top of it, since replay for GOOSE will extend that same code path.
