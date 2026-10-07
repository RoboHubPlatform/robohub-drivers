# DS18B20 temperature sensor module

An eval / breakout module carrying a Maxim DS18B20 1-Wire temperature sensor.

It is the driver-IC counterpart of a platform board: where a platform board's
support package says which MCU pin its peripherals reach, this says which 1-Wire
bus the module's data line is on and how the part is configured. The MCU pin the
wire lands on belongs to the platform board, so the same module is fitted to a
Pico, an STM32 or an ESP32.

## Signals

| Signal | Meaning |
|--------|---------|
| VCC | Supply, 3.0 V to 5.5 V |
| GND | Ground |
| DQ | 1-Wire data, with a pull-up to VCC |

The bus needs a pull-up on DQ — usually 4.7 kOhm to VCC. Many breakout boards
carry it; one that does not needs it fitted, or the line never returns high and
no device is seen.

## Defaults

| Setting | Value |
|---------|-------|
| 1-Wire bus | `onewire0` |
| Timer | `system_timer` |
| Resolution | 12 bits (750 ms per conversion) |
| Alarm high | +30 C |
| Alarm low | 0 C |

`bsp.h` names them, and a board where they differ passes its own configuration.

## Driver

`robohub-driver-ds18b20` — the platform-agnostic DS18B20 driver under
`drivers/temperature/ds18b20`. Selecting `RH_BOARD_DS18B20_MODULE` brings it in,
together with the 1-Wire bus and timer interfaces it needs.

## Wiring

For the Pico 2W, the module's data line is on **GP28, physical pin 34**, which is
what `robohub-pico`'s board support package registers as `onewire0`
(`RH_ONEWIRE0_PIN`). The three wires:

| Module | Pico 2W |
|--------|---------|
| VCC | 3V3(OUT), physical pin 36 |
| GND | GND, e.g. physical pin 33 or 38 |
| DQ | GP28, physical pin 34 |
| pull-up | 4.7 kOhm from DQ to 3V3 (the breakout usually carries it) |

The pin is the platform board's to choose — a module wired elsewhere changes
`RH_ONEWIRE0_PIN` in the platform board and nothing in this module.
