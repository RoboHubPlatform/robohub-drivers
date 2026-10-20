# ky-015-temperature-humidity-sensor eval board

A KY-015 temperature and humidity sensor: a DHT11 on a single-wire bus.

## Pinout

| Pin | Carries |
|-----|---------|
| s | digital_io |
| vcc | power |
| gnd | ground |

## Driver

`robohub-driver-dht11` — see `drivers/environmental/dht11`. Selecting `RH_BOARD_KY_015_TEMPERATURE_HUMIDITY_SENSOR_MODULE` brings it in.
