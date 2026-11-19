# RoboHub Drivers

The drivers the devices are driven by, and the boards they are fitted to. A
driver is code for a part, written once and run on every platform that
implements the interfaces it is written against: it reaches its part through the
interfaces `robohub-common` declares (the register bus `RoboHubBus` with its I2C
and SPI backends, `RoboHubOneWire`, `RoboHubDHT`, GPIO, ADC, PWM) and presents
the driver interface of its kind, such as `RoboHubTemperature`. It names no
vendor SDK and no chip. See {ref}`robohub-common/index`.

Sensors live here. A device in `robohub-devices` composes drivers into the
equipment a node is part of — the smart plug is the relay and the DS18B20
together (see {ref}`robohub-devices/index`).

## The drivers

### Input
- **button-gpio**: button input via GPIO
- **analog-adc**: analog input via ADC
- **encoder-gpio**: rotary encoder via GPIO
- **pulse-adc**: pulse input via ADC
- **infrared-receiver-gpio**: infrared receiver via GPIO, decoding NEC — standard (8-bit address) and extended (16-bit address) — as `(address << 8) | command`

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

## The boards

`boards/` holds the modules a part is sold on — the KY-0xx sensor kit, the
DS18B20 module and waterproof probe, the BMP180 and DHT11 modules, the MAX485
RS485 transceiver module — grouped as the drivers are (`environmental`,
`input`, `light`, `output`, `temperature`, `transceiver`). A board names the
driver it is read with, its pins and its `signals:`.

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

## The examples

`examples/` holds programs that show the drivers on real boards, in C and in
Rho side by side: the DS18B20 (bare metal, with FreeRTOS, and the `telemetry`
node a gateway reads), the BMP180 and the DHT11. `examples/gateway/c` holds the
gateway firmwares built on the same parts: `repeater` (a Pico that measures and
forwards the boards on its UART), `relay`, and the host builds `relay-host` and
`uplink-host` (see {ref}`robohub-gateway/index`).

## Building

```sh
cd examples/temperature/ds18b20/c/telemetry
rho build -target pico2w .
```

## Testing

```sh
ROBOHUB_COMMON_DIR=/path/to/robohub-common \
ROBOHUB_ORCHESTRATOR_DIR=/path/to/robohub \
  tests/run.sh
```

`rho test .` runs the same host tests: `tests/tests.yml` declares them as a ctest
suite (`tests/CMakeLists.txt`), which finds robohub-common and robohub under
`ROBOHUB_WORKSPACE`, or beside this repository. They exercise the part drivers
over fake HAL halves, the infrared receiver against replayed NEC frames, and the
BMP180, DHT11 and DS18B20 protocols, including a part that does not answer
(`ROBOHUB_ERR_NOT_FOUND`).
