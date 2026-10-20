# ky-008-laser-transmitter eval board

A KY-008 laser transmitter: a diode you light by driving its signal.

## Pinout

| Pin | Carries |
|-----|---------|
| s | digital_out |
| vcc | power |
| gnd | ground |

## Driver

`robohub-driver-gpio-switch` — see `drivers/output/gpio-switch`. Selecting `RH_BOARD_KY_008_LASER_TRANSMITTER_MODULE` brings it in.
