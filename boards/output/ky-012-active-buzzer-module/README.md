# ky-012-active-buzzer eval board

A KY-012 active buzzer: it has its own oscillator, so it is driven on and off rather than at a frequency.

## Pinout

| Pin | Carries |
|-----|---------|
| s | digital_out |
| nc | not_connected |
| gnd | ground |

## Driver

`robohub-driver-gpio-switch` — see `drivers/output/gpio-switch`. Selecting `RH_BOARD_KY_012_ACTIVE_BUZZER_MODULE` brings it in.
