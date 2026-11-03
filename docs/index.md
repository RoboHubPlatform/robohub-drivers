# RoboHub Drivers

**Version:** 0.2.3

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

Boards and drivers carry environment tags in `metadata.tags`: `water` for a part
that can be immersed, `dirt` for one that can go into soil, and `air` for one
that measures ambient air. The waterproof DS18B20 probe carries `water` and
`dirt`; the bare DS18B20, BMP180 and DHT11 parts carry `air`.

### Transceiver
- **max485-module**: MAX485 RS485 transceiver module

## Signals

A board declares what a dashboard may bind to in a top-level `signals:` list.
Each item names an `id`, a `label`, a `direction` (input or output), a `unit`,
the gateway `field` it reads, and the widget `kinds` it can feed:

```yaml
signals:
  - id: temperature
    label: Temperature
    direction: input
    unit: "°C"
    field: temperature_c
    kinds: "numeric chart table"
```

A sensor is input-only, so it is not offered to a switch. A board without the
block falls back to its `functions:`, with the unit for each function; a signal
whose field the gateway does not carry yet reads empty rather than being
guessed.

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
