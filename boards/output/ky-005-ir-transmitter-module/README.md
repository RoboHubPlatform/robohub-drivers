# ky-005-ir-transmitter eval board

A KY-005 infrared transmitter on a PWM channel.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| s | pwm_out |

## Driver

`robohub-driver-infrared-pwm` — see `drivers/output/infrared-pwm`. Selecting `RH_BOARD_KY_005_IR_TRANSMITTER_MODULE` brings it in.
