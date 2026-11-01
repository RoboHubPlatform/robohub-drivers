# Changelog

All notable changes to this module are recorded here, one section per release.

## [0.2.2] - 2026-11-02

Sensor boards now declare how they reach a node in `board.bus`: the bus type and
instance, an I2C address or address range, an SPI chip select, or the exclusive
ADC/PWM/GPIO/UART instance. The Mesh node editor reads this, shares 1-Wire and
I2C lines, allocates an I2C address from a declared range and a chip select for
SPI, and reports a bus or address that is already in use. A conflicted module is
left out of the node image so the parent still builds.

The DHT11, BMP180 and thermistor drivers declare `capabilities` and, where they
have one, the example a node is built from. DHT11 or BMP180 can now be the first
module of a node, with further modules linked into the same image.

## [0.2.1] - 2026-11-02

A waterproof DS18B20 probe board: `boards/temperature/ds18b20-waterproof-probe-module`.
It uses the same driver and 1-Wire defaults as the breakout module, in a sealed
stainless-steel probe on a one-metre cable, so the same part can measure water,
soil or ambient air.

Boards and drivers now carry environment tags in `metadata.tags`: `water` for a
part that can be immersed, `dirt` for one that can go into soil, and `air` for
one that measures ambient air. The waterproof probe and the DS18B20 driver carry
`water` and `dirt`; the DS18B20, BMP180 and DHT11 drivers and their boards carry
`air`. A search over the manifests can select a part by the medium it is rated
for.

## [0.2.0] - 2026-11-01

The DS18B20 telemetry node is transport-aware: UART uses a hardware UART, USB
and Console use USB CDC, WiFi and Socket use the TCP sink, selected by
`CONFIG_RH_NODE_TRANSPORT`. The node retries the sensor every five seconds, so
one connected after boot recovers without a reset. Driver and board manifests
declare the capabilities and deployment metadata a Mesh planner reads.
