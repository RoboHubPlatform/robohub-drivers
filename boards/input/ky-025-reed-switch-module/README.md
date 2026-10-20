# ky-025-reed-switch eval board

A KY-025 reed switch: a digital contact and an analog output of the field strength.

## Pinout

| Pin | Carries |
|-----|---------|
| s | digital_in |
| vcc | power |
| gnd | ground |
| ao | analog_in |

## Driver

`robohub-driver-analog-adc` — see `drivers/input/analog-adc`. Selecting `RH_BOARD_KY_025_REED_SWITCH_MODULE` brings it in.
