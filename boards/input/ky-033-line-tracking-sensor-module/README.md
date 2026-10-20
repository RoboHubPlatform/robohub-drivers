# ky-033-line-tracking-sensor eval board

A KY-033 line-tracking sensor: one reflective detector that says whether it is over a dark line.

## Pinout

| Pin | Carries |
|-----|---------|
| gnd | ground |
| out | digital_in |
| vcc | power |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_033_LINE_TRACKING_SENSOR_MODULE` brings it in.
