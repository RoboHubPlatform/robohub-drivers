# ky-003-hall-magnetic-sensor eval board

A KY-003 hall magnetic sensor: a solid-state switch that is polarity sensitive.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| s | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_003_HALL_MAGNETIC_SENSOR_MODULE` brings it in.
