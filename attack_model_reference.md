# Attack Model Reference — Pre-implementation Notes for Slow DDoS / Replay / FDI

**Date:** 2026-07-20
**Purpose:** Architecture map compiled before starting three new threats (slow DDoS, replay,
false data injection) across DNP3, Modbus TCP, MMS, and GOOSE. Written so a fresh chat/session
per threat branch doesn't have to re-derive this from scratch. Not a record of completed work —
none of the three threats below exist yet.

See also: `ATTACKEXAMPLE.md` (user-facing config docs for existing MIM/DDoS attacks),
`iec61850_session_summary.md`, `modbus_tcp_session_summary.md`.

---

## 1. `AttackConf` is not a struct — it's a per-protocol ns-3 attribute

No `struct`/`class AttackConf` exists anywhere. "AttackConf" is the ns-3 `Attribute` name each
protocol `Application` subclass registers via `TypeId::AddAttribute`, backed by a plain
`std::string configFile` member. The same pattern is duplicated near-verbatim in all four
protocol application files:

- `RC/code/helics/modbus-application-new.cc:202-205`
- `RC/code/helics/dnp3-application-new.cc:272-275` (and `dnp3-application-new-Docker.cc:272`)
- `RC/code/helics/mms-application-new.cc:192-195`
- `RC/code/helics/goose-application-new.cc:220-223`

Default sentinel is the string `"NA"`. This is the historical source of the bug fixed in commit
`f031545` — a substring check (`configFile.find("NA") == npos`) false-matched any real path
containing "NA" (e.g. `.../NATIG/...`). MMS and GOOSE were written after that fix and already use
exact-equality (`configFile != "NA"`); DNP3/Modbus now do too.

`configFile` points to a topology-level attack-config JSON (e.g. `attack_config_goose_test.json`),
loaded via the identical `readMicroGridConfig(fpath, Json::Value&)` helper in each protocol file
(one-liner over jsoncpp `Json::Reader` — see `goose-application-new.cc:330-336`). The relevant
top-level key is `"MIM"`, an array where index `0` is reserved/empty and index `N` (matched to the
attacker's `ID` attribute) holds that attacker's fields. Production grid configs also carry
`attack_val`, `real_val`, `node_id`, `point_id`, `scenario_id`, `Start`/`End`, flattened at load
time into a `std::map<std::string,std::string> attack` keyed `"MIM-" + attackerID + "-" + field`.

**Sibling attributes** set alongside `AttackConf` on every protocol (kept identical across
protocols deliberately — this is what keeps cross-protocol detectability comparisons valid; see
full block at `goose-application-new.cc:150-253`):

| Attribute | Member | Purpose |
|---|---|---|
| `mitmFlag` | `mitm_flag` (bool) | Marks this app instance as the attacker |
| `AttackConf` | `configFile` (string) | Path to attack-config JSON, default `"NA"` |
| `ID` | `MIM_ID` (uint16_t) | Attacker index into `configObject["MIM"][ID]` |
| `AttackSelection` | `m_attackType` (uint16_t) | 2/4 = FDI on analog, 3 = forced binary control |
| `Value_attck` / `_max` / `_min` | `m_attack_point_val` / `m_attack_max` / `m_attack_min` | Injected value(s), comma-delimited per point |
| `NodeID` / `PointID` | `node_id` / `point_id` | Comma-delimited target node/point names |
| `RealVal` | `RealVal` | Value to restore after the attack window closes |
| `AttackStartTime` / `AttackEndTime` | `m_attackStartTime` / `m_attackEndTime` | Scheduling |
| `AttackChance` | `m_attackChance` (double) | Probability the attack fires on a given tick |

`StartVect`/`StopVect` (`std::vector<std::string>`) dedupe which start/stop times have already
been `Simulator::Schedule`d, so repeated packets don't re-schedule the same callback.

---

## 2. Existing attack-handling logic, per protocol

### DNP3 / Modbus / MMS: `handle_MIM` — in-path interception

These are TCP client-server protocols, so a literal MITM socket sits between master and
outstation. `handle_MIM` is dispatched from `handle_inside`/`HandleRead` when `mitm_flag` is true:
- DNP3: `RC/code/helics/dnp3-application-new.cc:1608-1899` (declared `dnp3-application-new.h:255`)
- Modbus: `RC/code/helics/modbus-application-new.cc:1633-1830s` (declared `modbus-application-new.h:261`; comment at 1651 notes it's ported near-verbatim from DNP3)
- MMS: `RC/code/helics/mms-application-new.cc:1571-1770s` (declared `mms-application-new.h:288`; comment at 1598 notes the same lineage)

Flow: decode intercepted packet → load/parse `AttackConf` JSON → map `node_id$point_id` to
`analog_point_names`/`binary_point_names` (substring `find`, not exact match — fragile, flagged in
existing comments) → roll `attack_chance` against `PointStart`/`PointStop` window → on success,
mutate payload per `attack_type` (2/4 = FDI value swap, 3 = forced control command via
`direct_operate`/`ControlOutputRelayBlock`) → schedule delayed `resetToRealValue` back to
`RealVal` → forward via `send_directly`/`send_directly_server`.

### GOOSE: `handle_rogue_publish` — necessary divergence

GOOSE is UDP multicast pub/sub — there's no in-path position to intercept, so a second
`GooseApplicationNew` instance (`mitmFlag=true`) acts as a **rogue publisher**, forging a frame
with an artificially advanced `stNum` to exploit GOOSE's real-world "newest stNum wins" rule.
Implementation: `RC/code/helics/goose-application-new.cc:1157-1248`, declared
`goose-application-new.h:198-257`. Same `attack_type` numbering (2/4/3) preserved for
cross-protocol comparability. Dispatch: `attack_data(int freq)`
(`goose-application-new.cc:1513-1542`) branches on `mitm_flag` to either
`scheduleRoguePublish(freq)` (self-rescheduling loop) or the legitimate publisher's
burst/heartbeat schedule.

**Extension points for building on this**: `EncodePDU`/`GoosePDU` (`goose-application-new.h:118-131`)
for forging/replaying frame content, `m_stNum`/`m_sqNum` tracking in `handle_rogue_publish` for
replay-timing/sequence-number games.

**Flag before extending**: `iec61850_session_summary.md:164` notes the rogue-publisher path in
the *production* topology (as opposed to the standalone `ns3-goose-mim-test.cc`) hasn't been
separately validated firing an actual attack window. Worth confirming before building replay/FDI
extensions on top of it.

### DDoS: topology-level only, not inside the application classes

**No `handle_DDoS` function exists anywhere.** DDoS is plain ns-3 flood traffic
(`OnOffHelper`/`BulkSendHelper`/`V4PingHelper` + `PacketSinkHelper`) wired directly in each
topology builder file (`ns3-helics-grid-dnp3-4G.cc`, `ns3-helics-grid-dnp3-5G.cc`,
`ns3-iec61850-goose-helics-grid.cc:874-1265`, etc.) via a dedicated `botNodes` `NodeContainer` —
completely orthogonal to the MIM/rogue-publish machinery. This pattern is **copy-pasted per
topology file**, not shared via a helper function.

Config shape (see `ATTACKEXAMPLE.md` for the user-facing field reference):
`NumberOfBots`, `threadsPerAttacker`, `Active`, `NodeType`/`NodeID` (target selection —
`CC`/`subNode`/`UE`/`MIM` node types), `endPoint`, `Start`/`End`/`TimeOn`/`TimeOff`,
`PacketSize`, `Rate`, `usePing` (ping-flood variant, marked "under development" in
`ATTACKEXAMPLE.md`). It's a flat-rate flood with an on/off duty cycle — **no low-and-slow
profile exists**.

---

## 3. What's genuinely new vs. what already exists

Grepped the full repo (`.cc/.h/.md/.py/.json`) for `replay`, `FDI`, `false data`, `slow.*ddos`,
`slowloris`, `low.rate` — findings:

- **FDI already exists conceptually**, as `attack_type` 2/4 inside `handle_MIM` (DNP3/Modbus/MMS)
  and `handle_rogue_publish` (GOOSE) — both inject an attacker-chosen analog value in place of the
  real one. See `RC/code/ns3-modbus-mim-test.cc:18` ("Minimal Modbus MIM (false data injection)
  attack test"), `modbus-application-new.cc:1813` ("False data injection on a register"). Building
  "FDI" as a threat likely means formalizing/extending this existing path (e.g., stealthy bounded
  injection or gradual drift vs. the current single-shot value swap), not building new plumbing.
- **Replay has zero prior implementation** anywhere in the codebase.
- **Slow DDoS has zero prior implementation.** Existing DDoS is flat-rate flood only.
- No session summary, handoff note, or TODO mentions any of these three as previously
  scoped/discussed work — this is a from-scratch design for all three.

---

## 4. Topology-level wiring pattern (how an attacker gets instantiated)

Gated by `includeMIM` in the grid config's `Simulation` block. GOOSE example
(`RC/code/ns3-iec61850-goose-helics-grid.cc:1058-1104`) reuses the already-installed per-microgrid
subscriber as the rogue role (no separate attacker node, since GOOSE has no in-path position):

```cpp
if (includeMIM == 1){
  for (int x = 0; x < val.size(); x++){
    int MIM_ID = std::stoi(val[x]) + 1;
    Ptr<GooseApplicationNew> rogue = gooseSubscriberByMicrogrid[MIM_ID-1];
    if (!rogue) { continue; }
    rogue->SetAttribute("mitmFlag", BooleanValue(true));
    rogue->SetAttribute("AttackConf", StringValue(configFileName));
    rogue->SetAttribute("ID", UintegerValue(MIM_ID));
    rogue->SetAttribute("AttackSelection", UintegerValue(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"])));
    // ... RealVal, Value_attck(_min/_max), NodeID, PointID, AttackStartTime, AttackEndTime ...
  }
}
```

DNP3/Modbus/MMS install a **separate** MIM node in-path instead (e.g.
`ns3-helics-grid-dnp3-4G.cc:1016`, `ns3-helics-grid-dnp3-5G.cc:1136`), same attribute-setting
pattern otherwise. The `val` (attacker IDs to activate) and `attack` map are built once near the
top of the topology `main()` and reused for both MIM/rogue setup and, independently, DDoS bot
setup later in the same file.

Attack start/stop scheduling happens **inside** the application's attack-handling function
(`handle_MIM`/`handle_rogue_publish`) via `Simulator::Schedule(..., &<Class>::set_attack, this,
true/false)`, deduped against `StartVect`/`StopVect` — not in the topology file. The topology
file's job is only to construct the attacker node/app and set the `AttackConf`-family attributes.

---

## 5. Open design questions for the three new threats

- **FDI**: is the goal to extend `attack_type` 2/4 (e.g., bounded/stealthy injection instead of a
  blunt value swap), or add a genuinely distinct attack type number? Needs a decision before
  touching code, since it changes whether this is a small diff (extend existing path in 4 files)
  or a new one.
- **Slow DDoS**: needs a protocol-agnostic definition first. Slowloris-style connection
  exhaustion is meaningful for DNP3/Modbus/MMS (TCP) but GOOSE is connectionless UDP multicast —
  what does "slow" mean there (e.g., artificially low publish rate to starve subscribers'
  timeout logic instead of connection exhaustion)? Also inherits the existing DDoS code's
  per-topology-file duplication — worth deciding whether to centralize before adding a second
  DDoS variant multiplies that duplication.
- **Replay**: for TCP protocols, hooks into `handle_MIM`'s packet path (capture + re-send a prior
  legitimate PDU). For GOOSE, would replay a captured `GoosePDU` — but GOOSE's own anti-replay
  design (`stNum`/`sqNum` "newest wins") means a naive replay of an old frame should legitimately
  fail against a spec-compliant subscriber. Worth deciding whether GOOSE's replay attack is
  "replay an old frame and confirm it's correctly rejected" (a detectability *baseline*, useful
  for the dissertation's comparison) vs. some sequence-number manipulation that actually succeeds.
