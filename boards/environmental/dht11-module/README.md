# DHT11 sensor module

An eval / breakout module carrying an Aosong DHT11 temperature and humidity
sensor.

It is the driver-IC counterpart of a platform board: where a platform board's
support package says which MCU pin its peripherals reach, this says which
single-wire line the module's data line is on and how often the part is asked.
The MCU pin the wire lands on belongs to the platform board, so the same module
is fitted to a Pico, an STM32 or an ESP32.

The DHT11 measures temperature and relative humidity and reports both in one
frame. It presents both driver interfaces, so a program written against
`RoboHubTemperature` or `RoboHubHumidity` can be handed this part without
changing a line.

## Signals

| Signal | Meaning |
|--------|---------|
| VCC | Supply, 3.3 V to 5.5 V |
| DATA | Single-wire data, with a pull-up to VCC |
| GND | Ground |

The data line needs a pull-up — usually 4.7 kOhm to VCC. Many breakouts carry it;
one that does not needs it fitted, or the line never returns high and no frame is
seen.

## Defaults

| Setting | Value |
|---------|-------|
| Single-wire line | `dht0` |
| Timer | `system_timer` |
| Sampling interval | 1000 ms |

`bsp.h` names them, and a board where they differ passes its own configuration.
The part cannot be sampled faster than about once a second; the driver enforces
the interval, and the timer is what it waits with.

## Driver

`robohub-driver-dht11` — the platform-agnostic DHT11 driver under
`drivers/environmental/dht11`. Selecting `RH_BOARD_DHT11_MODULE` brings it in,
together with the single-wire bus and timer interfaces it needs.

The pulse timing the wire is made of belongs to the platform's `RoboHubDHT`
implementation, not to the driver, so the same object file runs wherever that
bus exists. This module's BSP is the registry-specific layer: it looks the line
and timer up by name and hands them to the driver.

## Wiring

For the Pico 2W, the module's data line is on **GP22**, which is what
`robohub-pico`'s board support package registers as `dht0` (`RH_DHT0_PIN`). The
three wires:

| Module | Pico 2W |
|--------|---------|
| VCC | 3V3(OUT), physical pin 36 |
| DATA | GP22, physical pin 29 |
| GND | GND, e.g. physical pin 33 or 38 |
| pull-up | 4.7 kOhm from DATA to 3V3 (the breakout usually carries it) |

The pin is the platform board's to choose — a module wired elsewhere changes
`RH_DHT0_PIN` in the platform board and nothing in this module.
