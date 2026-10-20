# ky-035-analog-hall-magnetic-sensor eval board

A KY-035 analog hall sensor: the field it sees as a voltage.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| ao | analog_in |

## Driver

`robohub-driver-analog-adc` — see `drivers/input/analog-adc`. Selecting `RH_BOARD_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_MODULE` brings it in.
