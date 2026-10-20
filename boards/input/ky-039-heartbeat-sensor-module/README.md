# ky-039-heartbeat-sensor eval board

A KY-039 heartbeat sensor: a phototransistor that follows the pulse of blood.

## Pinout

| Pin | Carries |
|-----|---------|
| sig | analog_in |
| vcc | power |
| gnd | ground |

## Driver

`robohub-driver-pulse-adc` — see `drivers/input/pulse-adc`. Selecting `RH_BOARD_KY_039_HEARTBEAT_SENSOR_MODULE` brings it in.
