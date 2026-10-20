# BMP180 sensor module

An eval / breakout module carrying a Bosch BMP180 temperature, pressure and
altitude sensor.

It is the driver-IC counterpart of a platform board: where a platform board's
support package says which MCU pin its peripherals reach, this says which bus the
module is on and how the part is configured. The MCU pin the wires land on
belongs to the platform board, so the same module is fitted to a Pico, an STM32
or an ESP32.

The BMP180 measures temperature and pressure; altitude is computed from the
pressure against a sea-level reference. It presents all three driver interfaces,
so a program written against `RoboHubTemperature`, `RoboHubPressure` or
`RoboHubAltitude` can be handed this part without changing a line.

## Signals

| Signal | Meaning |
|--------|---------|
| VCC | Supply, 1.8 V to 3.6 V |
| GND | Ground |
| SDA | I2C data, or SPI MOSI |
| SCL | I2C clock, or SPI clock |
| SDO | I2C address select, or SPI MISO |
| CSB | Chip select (SPI only) |

The part is on I2C by default. With CSB tied high it answers at address `0x77`;
tied low (or on SPI) the address is `0x76`. SDA and SCL need pull-ups to VCC when
the bus does not already carry them.

## Defaults

| Setting | Value |
|---------|-------|
| Transport | I2C |
| Bus | `i2c0` |
| Address | `0x77` |
| Timer | `system_timer` |
| Oversampling | 0 (4.5 ms per pressure conversion) |
| Sea-level reference | 101325 Pa |
| Bus speed | 400 kHz |

`bsp.h` names them, and a board where they differ passes its own configuration.
The same part can be fitted on SPI instead by setting the transport to SPI,
naming the `spi0` controller and, when a separate pin drives it, the chip select.

## Driver

`robohub-driver-bmp180` — the platform-agnostic BMP180 driver under
`drivers/environmental/bmp180`. Selecting `RH_BOARD_BMP180_MODULE` brings it in,
together with the bus and timer interfaces it needs.

The driver is written against the common register bus (`RoboHubBus`), so it does
not know whether the module is fitted on I2C or SPI. This module's BSP is the
registry-specific layer: it looks the controller and timer up by name and hands
the driver a bound bus.

## Wiring

For the Pico 2W, the module's I2C bus is `i2c0`, which `robohub-pico`'s board
support package registers. The four wires:

| Module | Pico 2W |
|--------|---------|
| VCC | 3V3(OUT), physical pin 36 |
| GND | GND, e.g. physical pin 33 or 38 |
| SDA | GP4, physical pin 6 (I2C0 SDA) |
| SCL | GP5, physical pin 7 (I2C0 SCL) |

The pins are the platform board's to choose — a module wired elsewhere changes
the platform board and nothing in this module.
