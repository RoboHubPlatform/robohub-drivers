# ky-037-microphone-sound-sensor eval board

A KY-037 microphone sound sensor: the sound at the microphone as a level.

## Pinout

| Pin | Carries |
|-----|---------|
| sig | analog_in |
| vcc | power |
| gnd | ground |

## Driver

`robohub-driver-analog-adc` — see `drivers/input/analog-adc`. Selecting `RH_BOARD_KY_037_MICROPHONE_SOUND_SENSOR_MODULE` brings it in.
