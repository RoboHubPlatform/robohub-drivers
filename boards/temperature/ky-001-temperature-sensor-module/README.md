# ky-001-temperature-sensor eval board

A KY-001 temperature sensor: a DS18B20 on a 1-Wire bus.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| dq | onewire |

## Driver

`robohub-driver-ds18b20` — see `drivers/temperature/ds18b20`. Selecting `RH_BOARD_KY_001_TEMPERATURE_SENSOR_MODULE` brings it in.
