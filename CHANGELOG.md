# Changelog

All notable changes to this module are recorded here, one section per release.

## [0.2.0] - 2026-11-01

The DS18B20 telemetry node is transport-aware: UART uses a hardware UART, USB
and Console use USB CDC, WiFi and Socket use the TCP sink, selected by
`CONFIG_RH_NODE_TRANSPORT`. The node retries the sensor every five seconds, so
one connected after boot recovers without a reset. Driver and board manifests
declare the capabilities and deployment metadata a Mesh planner reads.
