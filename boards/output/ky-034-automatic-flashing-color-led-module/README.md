# ky-034-automatic-flashing-color-led eval board

A KY-034 seven-colour self-flashing LED: the sequence is inside the LED, so it is switched on and off as a whole.

## Pinout

| Pin | Carries |
|-----|---------|
| s | digital_out |
| gnd | ground |
| gnd2 | ground |

## Driver

`robohub-driver-gpio-switch` — see `drivers/output/gpio-switch`. Selecting `RH_BOARD_KY_034_AUTOMATIC_FLASHING_COLOR_LED_MODULE` brings it in.
