# Examples

Programs that show the drivers in this repository working on real boards.

The tree mirrors `drivers/`: one directory per category, then one per device,
and inside it the same application written in each language the toolchain
builds — `c/` and `rho/`. Both do the same thing, so the two can be read side by
side.

```
examples/
    temperature/
        ds18b20/
            c/          the DS18B20 program, in C
            rho/        the DS18B20 program, in Rho
```

Each application reads the sensor every 1.5 seconds and writes the temperature
to the console, and each is built for a board the same way:

```sh
cd examples/temperature/ds18b20/c
rho build -target pico2w .
rho flash ...
```

The applications depend on a platform (`robohub-pico`, for the Pico 2W) and on
the eval module board under `boards/`, which brings in the driver. The platform
supplies the bus and the timer; the driver supplies the protocol; the program
supplies the loop.

Both have been run on a Pico 2W with the DS18B20 module on GP28 (physical pin 34)
and print a reading every 1.5 seconds. The wiring is in the module's README.
