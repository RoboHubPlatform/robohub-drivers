# ky-026-flame-sensor eval board

A KY-026 flame sensor: the flame it sees as a voltage.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| ao | analog_in |

## Driver

`robohub-driver-analog-adc` — see `drivers/input/analog-adc`. Selecting `RH_BOARD_KY_026_FLAME_SENSOR_MODULE` brings it in.
