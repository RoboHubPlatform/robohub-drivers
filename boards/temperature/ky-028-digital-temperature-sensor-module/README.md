# ky-028-digital-temperature-sensor eval board

A KY-028 digital temperature sensor: a thermistor and a comparator, which says when the temperature crosses the threshold its trimmer sets. Its analog output is a relative reading, not a temperature, so the threshold is the part that is used.

## Pinout

| Pin | Carries |
|-----|---------|
| g | ground |
| vcc | power |
| ao | analog_in |
| d0 | digital_in |

## Driver

`robohub-driver-button-gpio` — see `drivers/input/button-gpio`. Selecting `RH_BOARD_KY_028_DIGITAL_TEMPERATURE_SENSOR_MODULE` brings it in.
