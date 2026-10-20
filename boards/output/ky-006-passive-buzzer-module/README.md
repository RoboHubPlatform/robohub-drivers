# ky-006-passive-buzzer eval board

A KY-006 passive buzzer on a PWM channel.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| s | pwm_out |

## Driver

`robohub-driver-tone-pwm` — see `drivers/output/tone-pwm`. Selecting `RH_BOARD_KY_006_PASSIVE_BUZZER_MODULE` brings it in.
