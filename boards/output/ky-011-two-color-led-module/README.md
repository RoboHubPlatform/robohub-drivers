# ky-011-two-color-led eval board

A KY-011 two-colour LED on two PWM channels: red and green, with no blue.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| r | pwm_out |
| g | pwm_out |

## Driver

`robohub-driver-rgb-pwm` — see `drivers/output/rgb-pwm`. Selecting `RH_BOARD_KY_011_TWO_COLOR_LED_MODULE` brings it in.
