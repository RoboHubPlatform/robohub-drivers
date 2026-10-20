# ky-020-tilt-switch eval board

A KY-020 tilt switch: a ball switch on a GPIO.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| s | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_020_TILT_SWITCH_MODULE` brings it in.
