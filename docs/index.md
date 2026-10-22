# RoboHub Drivers

The drivers the devices are driven by, and the boards they are fitted to. Each
driver implements the interfaces declared by `robohub-common`.

## The drivers

### Input
- **button-gpio**: button input via GPIO
- **analog-adc**: analog input via ADC
- **encoder-gpio**: rotary encoder via GPIO
- **pulse-adc**: pulse input via ADC
- **infrared-receiver-gpio**: infrared receiver via GPIO

### Output
- **tone-pwm**: tone generation via PWM
- **rgb-pwm**: RGB LED via PWM
- **infrared-pwm**: infrared output via PWM
- **gpio-switch**: GPIO switch output

### Light
- **photoresistor-adc**: photoresistor via ADC

### Temperature
- **thermistor-adc**: thermistor via ADC
- **ds18b20**: DS18B20 temperature sensor

### Environmental
- **bmp180**: BMP180 barometric pressure sensor
- **dht11**: DHT11 temperature and humidity sensor

### Transceiver
- **max485-module**: MAX485 RS485 transceiver module

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
