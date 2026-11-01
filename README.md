# robohub-drivers

Device drivers for the RoboHub Platform: code for a part, written once and run
on every platform that implements the interfaces it is written against.

A driver here is platform-agnostic. It reaches its device through the interfaces
`robohub-common` declares and implements the driver interface its own kind
declares, such as `RoboHubTemperature`. It names no vendor SDK and no chip, so
the same part runs on an RP2350, an STM32 or an ESP32 because each platform
implements the interface, not because the driver knows about any of them.

## The three layers of a driver

A driver is built from three layers, and only the last is written per device.
The first two are shared and live in `robohub-common`, which is what keeps a
driver from carrying any bus code of its own.

| Layer | What it is | Where it lives |
|-------|------------|----------------|
| Bus specific | The common register bus, `RoboHubBus`, and its I2C and SPI backends. The only code that knows a wire. | `robohub-common` — `include/robohub/hal/bus.h` |
| Registry specific | Resolving a controller by the name a board registered it under and binding it as a bus. | `robohub-common` — `robohub_bus_i2c_open` / `robohub_bus_spi_open`, used by each board's BSP |
| Driver | The part's protocol, written against `RoboHubBus` and the driver interface it presents. Knows neither I2C nor SPI. | `drivers/<category>/<device>/` |

### The common bus

Most parts are reached either over I2C or over SPI, and what differs is only how
a register is addressed. `RoboHubBus` is one contract — a register read and a
register write — with an I2C backend and an SPI backend behind it. A driver is
written against the contract, so the same object file runs whether the part was
fitted on I2C or on SPI: changing the wire is a change of board, not of driver.

The backend that knows the wire lives in `robohub-common`. So does the layer
that resolves a controller by the name a board registered it under, so a board's
support package is a join — `robohub_bus_i2c_open(&bus, "i2c0", 0x77, 400000)` —
and not per-driver glue.

A 1-Wire part is reached directly through `RoboHubOneWire`, which is its own bus
interface and is not register-shaped. So is a single-wire, timing-only part such
as a DHT11, through `RoboHubDHT`: the pulse lengths are the message, the platform
produces and measures them, and the driver speaks only the frame. Each is
layered the same way as the register bus — the interface comes from
`robohub-common`, the board resolves it, and the driver is written against it.

Not every wire is the common bus, and that is deliberate. `RoboHubBus` is for
parts whose protocol is a register read and a register write; a part whose
protocol is a pulse width gets an interface that says so rather than a bus that
would have to pretend.

## Driver interfaces

A program is written against a driver interface, not a part number. It asks for
a `RoboHubTemperature`, a `RoboHubPressure`, a `RoboHubAltitude` or a
`RoboHubHumidity`, is handed whichever part the board carries, and reads from
it. Swapping the part is then a change of what is compiled in, not a change of
the program.

A part that measures more than one thing presents more than one interface. The
BMP180 is a thermometer, a barometer and an altimeter at once; the DHT11 is a
thermometer and a hygrometer at once. A program that wants a temperature sensor
and one that wants an altimeter or a humidity reading are each handed the part
that has them, and neither names it.

## Finding a driver by what it does

A sensor usually belongs to more than one category, so a directory is a poor
index. Every driver lists the driver interfaces it presents in a `functions:`
list in its own `module.yml`, and the collection's own manifest declares the
vocabulary those names are drawn from:

```yaml
functions: [temperature, pressure, altitude]
```

A search for a temperature sensor then finds the mixed parts too, because they
say `temperature` as well as `pressure` and `altitude`, rather than being hidden
under one directory.

## Layout

```
drivers/<category>/<device>/
    inc/            headers a program includes
    src/            the implementation
    module.yml      the module's identity, its functions, and its build
    Kconfig         the symbol that switches it on in an application
    CMakeLists.txt

boards/<category>/<module>/
    bsp/            the module's registry layer: bind the bus, configure the part
    module.yml
    Kconfig
    README.md

examples/<category>/<device>/
    c/              the same application, in C
    rho/            the same application, in Rho
```

The examples mirror the drivers tree: one directory per category, then one per
device, so a driver and the programs that show it working are found in the same
place.

## Using a driver

An application declares the board module it fitted, which brings in the driver
and names the platform it runs on. The board's BSP joins the platform's bus to
the driver; the program reads the device through the driver interface.

```c
RoboHubBusI2C bus;
robohub_bus_i2c_open(&bus, "i2c0", 0x77, 400000);   /* registry layer */

RoboHubBMP180 sensor;
RoboHubBMP180Config config = { .bus = &bus.bus };
robohub_bmp180_init(&sensor, &config);

float celsius;
sensor.temperature.get_celsius(&sensor.temperature, &celsius);
```

The `RoboHubBus` is the platform's controller, looked up by the name its board
support package registered it under and bound by the board. The device is a value
the caller owns: there is no allocation and nothing to free.

## Status

The DS18B20 temperature driver, the BMP180 temperature/pressure/altitude driver
and the DHT11 temperature/humidity driver build. The DS18B20 example runs on a
Pico 2W with the module on GP28 (physical pin 34); the wiring is in each module's
README. `robohub-pico` implements the `RoboHubDHT` single-wire HAL on GP22, so
the DHT11 example builds against it as well.

## Starting a driver

A driver, and the example that uses it, are started from this repository rather
than copied from one:

```sh
rho init --project . --category environmental --name bmp280 \
         --set functions=temperature,pressure \
         --description "Bosch BMP280 temperature and pressure sensor" \
         driver drivers/environmental/bmp280
```

The root `module.yml` is what makes that work: it is a `collection`, and its
`generate:` section names the generator, the variables it asks for, and the two
directories it writes — the driver under `drivers/<category>/<name>` and its
example under `examples/<category>/<name>/c`. The templates are in `templates/`,
and `{{name}}`, `{{category}}`, `{{functions}}` and `{{description}}` are
substituted in the file names and their contents. Run `rho init --project .` with
no arguments to see what is on offer.

## Simple parts and eval boards

Not every part has a protocol. A button, a light sensor, a buzzer, an LED and an
infrared transmitter are a pin or a channel each, and their drivers are the
interface over that pin: `button-gpio` (a discrete contact), `photoresistor-adc`
(a light level), `thermistor-adc` (a temperature), `analog-adc` (a level),
`tone-pwm` (a frequency), `rgb-pwm` (a colour), `infrared-pwm` (a mark/space
sequence on a carrier), `infrared-receiver-gpio` (a decoded code),
`encoder-gpio` (steps and direction), `pulse-adc` (a rate) and `gpio-switch` (an
output). Each is reusable for any part of its kind, and each is in the category
of what it is — `input`, `output`, `light`, `temperature`.

The eval boards carry one of those parts and wire it. Every board's manifest
carries its **pinout** — the connector positions and what each carries, and the
polarity where it matters — so a person wiring the board reads it off the board
rather than off a page:

| Board | Driver | Function |
|-------|--------|----------|
| `boards/input/ky-004-key-switch-module` | `button-gpio` | button |
| `boards/input/ky-020-tilt-switch-module` | `button-gpio` | button |
| `boards/input/ky-002-vibration-switch-module` | `button-gpio` | button |
| `boards/input/ky-031-knock-sensor-module` | `button-gpio` | button |
| `boards/input/ky-021-mini-magnetic-reed-switch-module` | `button-gpio` | button |
| `boards/input/ky-003-hall-magnetic-sensor-module` | `button-gpio` | button |
| `boards/input/ky-010-photo-interrupter-module` | `button-gpio` | button |
| `boards/input/ky-025-reed-switch-module` | `analog-adc` | analog, button |
| `boards/input/ky-022-infrared-receiver-module` | `infrared-receiver-gpio` | infrared |
| `boards/input/ky-040-rotary-encoder-module` | `encoder-gpio` | encoder |
| `boards/input/ky-039-heartbeat-sensor-module` | `pulse-adc` | pulse |
| `boards/output/ky-006-passive-buzzer-module` | `tone-pwm` | tone |
| `boards/output/ky-011-two-color-led-module` | `rgb-pwm` | color |
| `boards/output/ky-029-dual-color-led-module` | `rgb-pwm` | color |
| `boards/output/ky-009-rgb-led-module` | `rgb-pwm` | color |
| `boards/output/ky-016-rgb-full-color-led-module` | `rgb-pwm` | color |
| `boards/output/ky-012-active-buzzer-module` | `gpio-switch` | switch |
| `boards/output/ky-034-automatic-flashing-color-led-module` | `gpio-switch` | color |
| `boards/output/ky-027-magic-light-cup-module` | `gpio-switch` | switch, button |
| `boards/light/ky-018-photoresistor-module` | `photoresistor-adc` | light |
| `boards/temperature/ky-013-analog-temperature-sensor-module` | `thermistor-adc` | temperature |
| `boards/temperature/ky-001-temperature-sensor-module` | `ds18b20` | temperature |
| `boards/temperature/ky-028-digital-temperature-sensor-module` | `button-gpio` | button |
| `boards/environmental/ky-015-temperature-humidity-sensor-module` | `dht11` | temperature, humidity |
| `boards/temperature/ds18b20-temperature-module` | `ds18b20` | temperature |
| `boards/temperature/ds18b20-waterproof-probe-module` | `ds18b20` | temperature |
| `boards/environmental/bmp180-module` | `bmp180` | temperature, pressure, altitude |
| `boards/environmental/dht11-module` | `dht11` | temperature, humidity |
| `boards/input/ky-032-infrared-obstacle-avoidance-sensor-module` | `button-gpio` | button |
| `boards/input/ky-033-line-tracking-sensor-module` | `button-gpio` | button |
| `boards/input/ky-036-metal-touch-sensor-module` | `button-gpio` | button |
| `boards/input/ky-023-joystick-dual-axis-module` | `analog-adc`, `button-gpio` | analog, button |
| `boards/input/ky-024-linear-magnetic-hall-module` | `analog-adc` | analog |
| `boards/input/ky-026-flame-sensor-module` | `analog-adc` | analog |
| `boards/input/ky-037-microphone-sound-sensor-module` | `analog-adc` | analog |
| `boards/input/ky-038-microphone-sound-sensor-module` | `analog-adc` | analog |
| `boards/output/ky-008-laser-transmitter-module` | `gpio-switch` | switch |
| `boards/output/ky-019-5v-relay-module` | `gpio-switch` | switch |

A board's support package resolves its interface from the registry and hands it
to the driver, so an application fits the board without repeating which pin or
channel it is on. The wiring is a macro, so a board overrides it from the build.

## Environment tags

A board or driver that is rated for a medium carries one of three tags in
`metadata.tags`, so a water, garden or air application can find the parts that
belong to it:

| Tag | Means | Carried by |
|-----|-------|------------|
| `water` | can be immersed in water | `ds18b20-waterproof-probe-module`, `robohub-driver-ds18b20` |
| `dirt` | can be pushed into or buried in soil | `ds18b20-waterproof-probe-module`, `robohub-driver-ds18b20` |
| `air` | measures ambient air | the DS18B20, BMP180 and DHT11 drivers; their boards and the KY-001, KY-013, KY-015 and KY-028 boards |

The tags are a claim about the part, not a substitute for its rating: the
waterproof probe is sealed for immersion but is not a pressure sensor, and the
bare breakouts are for air. A part with no tag is not rated for any of the three
media.

## Bus metadata

A sensor board says how it reaches the node in `board.bus`, so a node with more
than one module can share buses without guessing:

```yaml
board:
  bus:
    type: i2c
    instance: i2c0
    address: "0x77"        # a part with one address
    addresses: "0x76-0x77" # or the range it can be configured in
    cs: auto               # SPI: the node picks a chip select
```

1-Wire and I2C modules share their lines; an I2C module with a range is assigned
a free address by the planner, and two fixed addresses on one bus are reported
as a conflict. SPI modules share the bus and get a distinct chip select. ADC,
PWM, GPIO and UART instances are exclusive. A module that does not declare a bus
is still listed, with no claim made about how it connects.
A board may carry more than one part — the joystick carries two axes and a
switch — and binds each of them.

Some of these parts are not what their name suggests, and the manufacturer's
sheet is the one that says so: the KY-019 relay is **active-HIGH** (the opposite
of the blue optocoupler boards), the KY-012 buzzer's middle pin is not
connected, the KY-015 is a DHT11 and the KY-001 a DS18B20, the KY-028's analog
output is a relative reading and its useful half is the threshold switch, and the
KY-034 flashes by itself and can only be switched on and off.

## Tests

The part drivers have host tests over fake GPIO, PWM, ADC and timer halves under
`tests/`, run with:

```sh
ROBOHUB_COMMON_DIR=/path/to/robohub-common tests/run.sh
```

## Release history

[CHANGELOG.md](CHANGELOG.md) — the changes in 0.2.0 and earlier releases.
