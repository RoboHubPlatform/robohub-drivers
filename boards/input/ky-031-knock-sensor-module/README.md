# ky-031-knock-sensor eval board

A KY-031 knock sensor: a piezo knock contact on a GPIO.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| s | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_031_KNOCK_SENSOR_MODULE` brings it in.
