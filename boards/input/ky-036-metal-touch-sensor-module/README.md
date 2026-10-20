# ky-036-metal-touch-sensor eval board

A KY-036 metal-touch sensor: a capacitive pad that says when it is touched.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| sig | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_036_METAL_TOUCH_SENSOR_MODULE` brings it in.
