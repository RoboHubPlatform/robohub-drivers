# ky-010-photo-interrupter eval board

A KY-010 photo interrupter: a light barrier whose output changes when the beam is broken.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| s | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_010_PHOTO_INTERRUPTER_MODULE` brings it in.
