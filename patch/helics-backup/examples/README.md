# Examples

Here's a quick overview of the examples and what they are showing. If you have improvements for the examples,
PRs are welcome.

The general process for running the examples is start up a `helics_broker` that expects two federates to
connect, then start the matching `ns3-*` and `fed-*` example program -- for example, `ns3-filter` and
`fed-filter`.

* `fed-sndrcv`/`ns3-sndrcv`: an example showing how messages sent using a HELICS endpoint can be
   sent across a simulated ns-3 network as an intermediary. Uses the `StaticSink` and `StaticSource`
   helics-ns3 applications to create HELICS endpoints that tie into the ns-3 network.
* `fed-filter`/`ns3-filter`: an example showing how messages sent using a HELICS endpoint can be
   intercepted using a HELICS filter and sent across a simulated ns-3 network. Uses the `Filter`
   helics-ns3 application to intercept messages and send them through the ns-3 network instead.
* `fed-pubsub`: a basic example showing how to use the `helics_federate` to register
   publications and subscriptions. Can be run with the `ns3-sndrcv` federate. Integrating
   these with the simulated ns-3 network is not shown as it is very use case dependent.
   Some (unreleased) use cases have written custom `ns3::Application` classes.
