# Changelog

All notable changes to this module are recorded here, one section per release.

## [0.5.7] - 2026-11-20

The DS18B20 telemetry example takes updates over its link (DRV-TEMP-0005): with the mesh's update key in its build (`robohub_update_key.h`) and the managed layer chosen (`CONFIG_RH_NODE_MANAGED`), it hands robohub-node's managed layer its link, its uid and its firmware version; without them it builds as before.

## [0.5.6] - 2026-11-20

The examples' protocol copies know the update messages (robohub-protocol 1.5.0).

## [0.5.5] - 2026-11-20

The DS18B20 telemetry example — the firmware Mesh builds for a sensor board — reports the board's health: its link sends `HEALTH` once per boot and on a `DIAG_REQUEST` (robohub-node link 0.5.0), naming the board's area and node and `CONFIG_RH_FIRMWARE_VERSION` (0 when unset).

## [0.5.4] - 2026-11-20

The gateway examples' and the DS18B20 telemetry example's protocol copies know `HEALTH` (0x0403) and `ResetReason` (robohub-protocol 1.4.0).

## [0.5.3] - 2026-11-20

The examples name robohub-core, robohub-transport, robohub-telemetry and robohub-freertos at their new paths in the robohub-node collection (`core/`, `link/`, `telemetry/`, `kernels/freertos/`; robohub ADR-0020); CI checks out robohub-node instead of the four repositories.

## [0.5.2] - 2026-11-19

The infrared receiver decodes real remotes (infrared-receiver-gpio 0.1.1,
DRV-IN-0005, DRV-IN-0006). It decoded nothing a remote sends: it compared the
first sixteen bits of a frame with the inverse of the last sixteen, which no NEC
frame satisfies (the bytes are address, address inverse, command, command
inverse), so every standard frame was refused, and most remotes now send the
extended form, whose second byte is the high byte of a 16-bit address with no
complement at all. Two more faults kept it deaf before the check was reached: the
capture recorded a "gap" at every poll the line was idle rather than at each
edge, and it compared a bit's space with the whole bit period (1125/2250 µs)
instead of the space alone (563/1688 µs). The capture now waits for the first mark
and times each level until one outlasts 10 ms; the decode checks the header, the
32 bits and the closing mark, refuses a frame whose command does not invert, and
reads the address as 8 bits when its second byte inverts it and as 16 bits when
it does not. The code is `(address << 8) | command`. A repeat code is still no
new code. `tests/test_ir_receiver.c` replays standard, extended, corrupted, cut-short
and repeat frames on a fake pin.

The BMP180 and the DHT11 report a part that is not there as
`ROBOHUB_ERR_NOT_FOUND`, as their headers said (bmp180 0.1.1, dht11 0.1.1,
DRV-ENV-0006). The BMP180 returned the bus's own error for an unacknowledged
address and `ROBOHUB_ERR_PROTOCOL` for an SPI line nothing drives; it now returns
not found when the identity read fails with an I/O error or a timeout, or reads
0x00 or 0xFF, and keeps the protocol error for a part of another identity. The
DHT11 passed a bus timeout through and read an all-zero frame — what an undriven
line samples as, and whose checksum checks — as 0 °C and 0 % humidity; both are
now not found.

The GPIO switch's function `switch` is in the collection's vocabulary: it is the
on/off output robohub-common declares as `RoboHubSwitch` and as the device kind
`switch`, and the relay board built on the driver claims it too.

CI runs this collection's tests: `tests/tests.yml` declares the host tests
(`tests/run.sh`, through a CTest project in `tests/CMakeLists.txt`) as a ctest
suite, so `rho test .` runs them. `tests/test_environmental.c` adds host tests for
the BMP180 (the datasheet's worked example: 15.0 °C, 69964 Pa), the DHT11 (frame,
checksum, pacing) and the DS18B20 (temperatures, CRC-8 checked against a real
scratchpad, resolution masking, an empty bus).

The board contract says what the boards carry and what Mesh reads (DRV-BRD-0004).
`contracts.board` (0.2.3) and the README required `capabilities` and
`board.driver` of every board; no board but the two DS18B20 ones had
`capabilities`, and Mesh does not need it of a board — the driver declares it,
a board's summary falls back to its description, and only a board with
`capabilities.sensor: true` is listed among the sensor drivers — so it is now
optional. `board.driver` is required of a board that depends on a driver, which
Mesh compiles and links; `max485-module` binds its RS485 itself and depends on
none. Three boards (KY-016, KY-017, KY-029), and the board template, spelled
the block `connector:`, which nothing reads, with only power and ground: they
now carry `connectors` with the signal lines their support packages bind.
`tests/board_manifests_test.py`, a `boards` suite in `tests/tests.yml`, checks
every board manifest against `contracts.board` on each `rho test .`.

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

## [0.5.1] - 2026-11-14

The protocol copies know `SESSION_RESET`; the relays pass it to their boards inside `RELAYED`.
