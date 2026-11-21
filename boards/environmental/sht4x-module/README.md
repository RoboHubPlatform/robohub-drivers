# SHT4x sensor module

A breakout carrying a Sensirion SHT40, SHT41 or SHT45 temperature and humidity
sensor on I2C (DRV-BRD-0005, robohub ADR-0026). Outdoors, use an SHT45 with a
filter cap in a vented enclosure, out of direct sun.

| Wire | Pico 2 W | LOLIN32 Lite |
|------|----------|--------------|
| VCC  | 3V3      | 3V3          |
| GND  | GND      | GND          |
| SDA  | GP8      | GPIO23       |
| SCL  | GP9      | GPIO19       |

Most breakouts carry the I2C pull-ups; a bare part needs 4.7 kΩ to 3V3 on SDA
and SCL. The part answers at 0x44 (an -BD1B variant at 0x45: set `address`).

Fitted to a node in Mesh, the module is read every poll through
`robohub_sht4x_module_init` and the driver's temperature and humidity
interfaces; each reading is checked by its CRC before it is published.
