# ky-021-mini-magnetic-reed-switch eval board

A KY-021 mini magnetic reed switch: a normally open contact that reads low when a magnet is near.

## Pinout

| Pin | Carries |
|-----|---------|
| s | digital_in |
| vcc | power |
| gnd | ground |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_021_MINI_MAGNETIC_REED_SWITCH_MODULE` brings it in.
