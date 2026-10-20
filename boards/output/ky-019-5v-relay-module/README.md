# ky-019-5v-relay eval board

A KY-019 5V relay: one channel that switches a load. It is active-HIGH — the relay pulls in when the signal is high, the opposite of the blue optocoupler boards.

## Pinout

| Pin | Carries |
|-----|---------|
| s | digital_out |
| vcc | power |
| gnd | ground |
| nc | relay_nc |
| com | relay_com |
| no | relay_no |

## Driver

`robohub-driver-gpio-switch` — see `drivers/output/gpio-switch`. Selecting `RH_BOARD_KY_019_5V_RELAY_MODULE` brings it in.
