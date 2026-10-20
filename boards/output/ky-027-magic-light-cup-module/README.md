# ky-027-magic-light-cup eval board

A KY-027 magic light cup: a mercury tilt switch and an LED on one small board, sold as a pair.

## Pinout

| Pin | Carries |
|-----|---------|
| g | ground |
| vcc | power |
| s | digital_in |
| l | digital_out |

## Driver

`robohub-driver-gpio-switch` — see `drivers/output/gpio-switch`. Selecting `RH_BOARD_KY_027_MAGIC_LIGHT_CUP_MODULE` brings it in.
