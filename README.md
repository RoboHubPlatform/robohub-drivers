# robohub-drivers

Device drivers for the RoboHub Platform: code for a part, written once and run
on every platform that implements the interfaces it is written against.

A driver here is platform-agnostic. It reaches its device through the HAL
interfaces `robohub-common` declares — a GPIO, an I2C bus, a 1-Wire bus, a timer
— and it implements the driver interface its own kind declares, such as
`RoboHubTemperature`. It names no vendor SDK and no chip, so the same DS18B20
driver runs on an RP2350, an STM32 or an ESP32 because each platform implements
the bus, not because the driver knows about any of them.

## The layers

Four things meet when a program reads a sensor, and they are kept apart because
each is a different kind of statement:

| Layer | What it is | Where it lives |
|-------|------------|----------------|
| Interfaces | What a platform implements: GPIO, 1-Wire, timer | `robohub-common` (`include/robohub/hal/`) |
| Driver interfaces | What a device is: a temperature sensor, a display | `robohub-common` (`include/robohub/drivers/`) |
| Driver | The code for one part, over the HAL and implementing the driver interface | `drivers/<category>/<device>/` |
| Driver board | Which driver a carrier holds and how its signals are named | `boards/<module>/` |

A platform implements the HAL interfaces and never a driver interface; a driver
implements a driver interface and never a HAL interface. That is what lets a
program be written against `RoboHubTemperature` and be handed a DS18B20, an
LM75 or an on-chip sensor without changing a line.

## Layout

```
drivers/<category>/<device>/
    inc/            headers a program includes
    src/            the implementation
    module.yml      the module's identity and build
    Kconfig         the symbol that switches it on in an application
    CMakeLists.txt

boards/<module>/
    bsp/            the module's pin names and peripheral defaults
    module.yml
    Kconfig
    README.md

examples/<category>/<device>/
    c/              the same application, in C
    rho/            the same application, in Rho
```

The examples mirror the drivers tree: one directory per device, under the same
category, so a driver and the programs that show it working are found in the
same place.

## Using a driver

An application declares the board module it fitted, which brings in the driver,
and names the platform it runs on. It reads the sensor through the device
interface, not through the driver's own API:

```c
RoboHubDS18B20 sensor;
RoboHubDS18B20Config config = {
    .bus   = robohub_get_onewire("onewire0"),
    .timer = robohub_get_timer("system_timer"),
    .resolution_bits = 12,
};
robohub_ds18b20_init(&sensor, &config);

float celsius;
sensor.interface.get_celsius(&sensor.interface, &celsius);
```

The bus and the timer are the platform's, looked up by the name its board
support package registered them under. The device is a value the caller owns:
there is no allocation and nothing to free.

## Status

The DS18B20 driver, its eval module, and both applications build and run. On a
Pico 2W with the module on GP28 (physical pin 34), the C and Rho programs each
print a reading every 1.5 seconds; the wiring is in the module's README.

## Starting a driver

A driver, and the example that uses it, are started from this repository rather
than copied from one:

```sh
rho init --project . --category temperature --name sht31 \
         --description "Sensirion SHT31 temperature and humidity sensor" \
         driver drivers/temperature/sht31
```

The root `module.yml` is what makes that work: it is a `collection`, and its
`generate:` section names the generator, the variables it asks for, and the two
directories it writes — the driver under `drivers/<category>/<name>` and its
example under `examples/<category>/<name>/c`. The templates are in `templates/`,
and `{{name}}` and `{{category}}` are substituted in the file names and their
contents. Run `rho init --project .` with no arguments to see what is on offer.
