# ky-022-infrared-receiver eval board

A KY-022 infrared receiver: it hears the codes a remote sends.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| out | digital_in |

## Driver

`robohub-driver-infrared-receiver-gpio` — see `drivers/input/infrared-receiver-gpio`. Selecting `RH_BOARD_KY_022_INFRARED_RECEIVER_MODULE` brings it in.
