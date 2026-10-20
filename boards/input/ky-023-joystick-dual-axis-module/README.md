# ky-023-joystick-dual-axis eval board

A KY-023 dual-axis joystick: two potentiometers and a push switch.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| x | analog_in |
| y | analog_in |
| sw | digital_in |

The board carries **three** devices — the two axes and the switch — so its
support package binds each of them separately.

## Driver

`robohub-driver-analog-adc` and `robohub-driver-button-gpio`. Selecting
`RH_BOARD_KY_023_JOYSTICK_DUAL_AXIS_MODULE` brings them in.
