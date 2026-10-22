# RoboHub Drivers

The drivers the devices are driven by, and the boards they are fitted to. Each
driver implements the interfaces declared by `robohub-common`.

## The drivers

- **Input**: button (GPIO), analog (ADC), encoder (GPIO), pulse (ADC),
  infrared receiver (GPIO)
- **Output**: tone (PWM), RGB (PWM), infrared (PWM), GPIO switch
- **Light**: photoresistor (ADC)
- **Temperature**: thermistor (ADC)
- **Transceiver**: MAX485 module

## Building

```sh
cd nodes/gateway-node
rho build .
```

## Testing

```sh
ROBOHUB_COMMON_DIR=/path/to/robohub-common \\
ROBOHUB_ORCHESTRATOR_DIR=/path/to/robohub \\
  tests/run.sh
```
