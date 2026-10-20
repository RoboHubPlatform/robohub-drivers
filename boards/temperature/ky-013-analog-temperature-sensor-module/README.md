# ky-013-analog-temperature-sensor eval board

A KY-013 analog temperature sensor: a thermistor on an ADC channel.

## Pinout

| Pin | Carries |
|-----|---------|
| vcc | power |
| gnd | ground |
| ao | analog_in |

## Driver

`robohub-driver-thermistor-adc` — see `drivers/temperature/thermistor-adc`. Selecting `RH_BOARD_KY_013_ANALOG_TEMPERATURE_SENSOR_MODULE` brings it in.
