# Changelog

All notable changes to this module are recorded here, one section per release.

## [0.3.3] - 2026-11-14

The gateway and DS18B20 examples' protocol schemas are copies of robohub-protocol 1.0.0, written by its tools/sync.py; the generated C is unchanged.

## [0.3.2] - 2026-11-13

The DS18B20 driver writes its configuration only when the part does not hold it already: the scratchpad is read first, and the EEPROM is copied only when the alarms or the resolution differ. It wrote the EEPROM at every initialisation, so a node that re-initialised after a failed read wrote it twice a second — wearing the EEPROM, and on a parasite-powered probe keeping it failing.

## [0.3.1] - 2026-11-13

- The telemetry node on Wi-Fi comes back by itself: a connection the gateway
  closes, or Wi-Fi that drops, is joined and dialled again, waiting from a
  second up to half a minute between tries, and each new connection says hello
  again for a new session. Before, the board dialled once at boot and stayed
  silent until it was power-cycled; its message that it was "streaming over USB
  only" was not true, since nothing streamed.
- The Wi-Fi link and the relay uplink are sealed (2026-11-12, carried here).

## [0.3.0] - 2026-11-05

Drivers and board modules now describe their telemetry: a driver names the
device type and one line per readable signal, and a board module names its BSP
header, entry point and signals. Every sensor driver and every board module
with a read path carries the descriptor, and the telemetry node template keeps
its DS18B20 default while Mesh generates `sensors.c` for the modules in a plan.

## [0.2.3] - 2026-11-02

The collection's root manifest states the board module contract under
`contracts.board`: the discovery path (`boards/*/*/module.yml`), the fields a
board manifest carries, the `board.bus` fields a node plans with, the
`water`/`dirt`/`air` media tags, and the CMake exports a node links. The README
states the same contract for a person adding a board.

The DHT11, BMP180 and thermistor drivers declare `capabilities`, and DHT11 and
BMP180 carry the example and Kconfig a node is built from, so either can be the
first module of a node. A module attached to a node is linked into the node's
image; only the node is built and programmed.

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

The collection's root manifest states the board module contract under
`contracts.board`: where a board lives, the fields its manifest carries, the
`board.bus` fields a node plans with, and the `water`/`dirt`/`air` media tags.
The README says the same for a person adding a board.

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

## [0.4.0] - 2026-11-14

The C relay carries sealed boards end to end: their handshake and frames go up unchanged inside `RELAYED`, sealed under the relay's own session, and the parent's answers come back the same way; the relay holds no board keys. Its USB port takes nothing unless it is sealed. After the parent restarts it reconnects and announces again. relay-host reads its identity like other host nodes.

## [0.5.0] - 2026-11-14

The relays and the telemetry example start with the build's key epoch and scope. relay-host reads `<epoch>:<hex>` key files and can use its console as the parent link (`RHO_PARENT_LINK=console`), logging to stderr.
