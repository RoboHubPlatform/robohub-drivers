# ky-040-rotary-encoder eval board

A KY-040 rotary encoder: a knob that turns, with a push switch of its own.

## Pinout

| Pin | Carries |
|-----|---------|
| gnd | ground |
| vcc | power |
| sw | digital_in |
| dt | digital_in |
| clk | digital_in |

## Driver

`robohub-driver-encoder-gpio` — see `drivers/input/encoder-gpio`. Selecting `RH_BOARD_KY_040_ROTARY_ENCODER_MODULE` brings it in.
