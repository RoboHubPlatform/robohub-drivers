# ky-004-key-switch eval board

A KY-004 key switch: a push-button on a GPIO.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| s | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_004_KEY_SWITCH_MODULE` brings it in.
