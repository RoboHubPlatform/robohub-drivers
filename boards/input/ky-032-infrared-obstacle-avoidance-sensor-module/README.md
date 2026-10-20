# ky-032-infrared-obstacle-avoidance-sensor eval board

A KY-032 infrared obstacle-avoidance sensor: it says whether something is in front of it.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| out | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_032_INFRARED_OBSTACLE_AVOIDANCE_SENSOR_MODULE` brings it in.
