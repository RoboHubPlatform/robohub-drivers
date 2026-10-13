# Examples

Programs that show the drivers in this repository working on real boards.

The tree mirrors `drivers/`: one directory per category, then one per device,
and inside it the same application written in each language the toolchain
builds — `c/` and `rho/`. Both do the same thing, so the two can be read side by
side.

```
examples/
    temperature/
        ds18b20/
            c/
                bare-metal/   the DS18B20 program, in C, on the runtime alone
                freertos/     the same program, with FreeRTOS beneath the runtime
            rho/              the DS18B20 program, in Rho
    environmental/
        bmp180/
            c/                the BMP180 program, in C
            rho/              the BMP180 program, in Rho
        dht11/
            c/                the DHT11 program, in C
            rho/              the DHT11 program, in Rho
```

Each application reads the sensor every 1.5 seconds and writes the temperature
to the console. The C program is built on `robohub-core`: a sensor application
publishes a reading and a telemetry application subscribes to it, and neither
names the other. The two C variants differ by one dependency — `freertos/`
links `robohub-freertos`, so the same runtime runs its tasks as FreeRTOS tasks;
`bare-metal/` links nothing and runs one cooperative loop.

Build one for a board the same way:

```sh
cd examples/temperature/ds18b20/c/bare-metal
rho build -target pico2w .
rho flash -target pico2w . build/output/c.elf
```

The applications depend on a platform (`robohub-pico`, for the Pico 2W) and on
the eval module board under `boards/`, which brings in the driver. The platform
supplies the bus and the timer; the driver supplies the protocol; the runtime
supplies the loop and the event bus; the program supplies the two applications.

The bare-metal variant has been run on a Pico 2W with the DS18B20 module on GP28
(physical pin 34) and prints a reading every 1.5 seconds. The wiring is in the
module's README. The FreeRTOS variant additionally needs the FreeRTOS kernel
fetched into `robohub-freertos/vendor/` and a `FreeRTOSConfig.h` for the board.

## Environmental

`examples/environmental/bmp180` shows a part that presents more than one driver
interface. The BMP180 measures temperature and pressure and computes altitude
from the pressure, so the same program prints all three. It is reached through
the common register bus, which the board's BSP resolves from the platform's I2C
bus, so the program names neither I2C nor a chip. It is the counterpart of the
DS18B20 example for a register device rather than a 1-Wire one.

`examples/environmental/dht11` shows a part on a wire that is neither a register
bus nor 1-Wire: the DHT11 reports temperature and relative humidity in one frame
on a single timed line. The pulse timing belongs to the platform's `RoboHubDHT`
implementation and the frame decoding to the driver, so the program names
neither a pin nor a microsecond.

## Gateways

Three of the examples are the three roles of a RoboHub tree, and they share one
wire (`robohub-gateway/schemas/link.rho`), so any of them can be the board
another of them gateways:

| Example | Role | Senses | Downstream | Upstream |
|---|---|---|---|---|
| `temperature/ds18b20/c/telemetry` | `node` | a DS18B20 | — | USB CDC |
| `gateway/c/repeater` | `edge` | a DS18B20 | `uart1` | USB CDC |
| `gateway/c/relay` | `relay` | — | `uart1` | USB CDC |

A reading names its device — an area and a node — so a gateway attributes it
without the link telling it, and no one keeps a source table. The edge measures
while it gateways; the relay only gateways. With a host on the USB CDC the
readings reach `robohub-gateway`; without one, the same firmware streams to
whatever reads the link. A relay with `CONFIG_RH_NET` also dials the gateway
above it over TCP, announcing the area it speaks for and re-framing each reading
it heard as an uplink reading — so a relay is a hop of a tree, and the gateway
at the top lists every board below it.

Wiring a board behind an edge or a relay is three wires: GP4 (UART1 TX) to the
board's RX, GP5 (UART1 RX) to the board's TX, and GND. The board runs the node
firmware with its own `CONFIG_RH_AREA_ID` and `CONFIG_RH_NODE_ID`.

A relay routes **down**: a command or a diagnostic the parent sends for a board
behind it goes down the downstream link with the parent's transaction id intact,
and the board's answer is carried back up. A relay answers for itself as node 0
of its area, and forwards anything addressed to another node. An edge — a
measuring gateway — answers for its own sensor and has the same downstream link,
and adopts the same routing when it needs it.

Each of the three answers a **diagnostic** (`DIAG_REQUEST`, `0x0401`) with the
runtime's counters and a **hardware smoke**: `robohub_core_smoke` probes the
clock for monotonicity and asks the registry whether each bus was brought up, so
a gateway can tell a board that stopped from a board whose OneWire bus was never
there. A probe the board does not have is absent rather than failed.
