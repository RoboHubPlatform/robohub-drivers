# ky-024-linear-magnetic-hall eval board

A KY-024 linear magnetic hall sensor: the field it sees as a voltage.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| ao | analog_in |

## Driver

`robohub-driver-analog-adc` — see `drivers/input/analog-adc`. Selecting `RH_BOARD_KY_024_LINEAR_MAGNETIC_HALL_MODULE` brings it in.
