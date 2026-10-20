# ky-009-rgb-led eval board

A KY-009 RGB full-colour LED on three PWM channels.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| r | pwm_out |
| g | pwm_out |
| b | pwm_out |

## Driver

`robohub-driver-rgb-pwm` — see `drivers/output/rgb-pwm`. Selecting `RH_BOARD_KY_009_RGB_LED_MODULE` brings it in.
