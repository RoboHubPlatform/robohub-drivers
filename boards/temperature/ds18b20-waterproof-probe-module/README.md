# DS18B20 waterproof temperature probe

A Maxim DS18B20 1-Wire temperature sensor in a sealed stainless-steel probe on a
one-metre cable. It is electrically the same part as the
`ds18b20-temperature-module` breakout and uses the same driver; the difference is
the package, which makes it usable where the bare board cannot go:

- **water** — immersed in a tank, a pipe or a pond,
- **dirt** — pushed into soil for a garden or a greenhouse,
- **air** — the same probe reads ambient temperature when it is not immersed.

The probe is not a pressure sensor and its cable entry is not rated for a
pressurised line; it is a temperature probe for water, soil and air.

## Signals

| Signal | Meaning |
|--------|---------|
| VCC | Supply, 3.0 V to 5.5 V |
| GND | Ground |
| DQ | 1-Wire data, with a pull-up to VCC |

The three wires are the probe's own colours on most assemblies (red VCC, black
GND, yellow DQ). The bus still needs a pull-up on DQ — usually 4.7 kOhm to VCC —
unless the carrier or the platform supplies one.

## Defaults

| Setting | Value |
|---------|-------|
| 1-Wire bus | `onewire0` |
| Timer | `system_timer` |
| Resolution | 12 bits (750 ms per conversion) |
| Alarm high | +30 C |
| Alarm low | 0 C |

`bsp/ds18b20_waterproof_probe_module.h` names them, and a board where they
differ passes its own configuration.

## Driver

`robohub-driver-ds18b20` — the platform-agnostic DS18B20 driver under
`drivers/temperature/ds18b20`. Selecting
`RH_BOARD_DS18B20_WATERPROOF_PROBE` brings it in, together with the 1-Wire bus
and timer interfaces it needs.

## Wiring

For the Pico 2W, the probe's data line is on **GP28, physical pin 34**, which is
what `robohub-pico`'s board support package registers as `onewire0`
(`RH_ONEWIRE0_PIN`):

| Probe | Pico 2W |
|-------|---------|
| VCC (red) | 3V3(OUT), physical pin 36 |
| GND (black) | GND, e.g. physical pin 33 or 38 |
| DQ (yellow) | GP28, physical pin 34 |
| pull-up | 4.7 kOhm from DQ to 3V3, unless already fitted |

The pin is the platform board's to choose — a probe wired elsewhere changes
`RH_ONEWIRE0_PIN` in the platform board and nothing in this module.

## Tags

The manifest carries `water` and `dirt` in `metadata.tags`, beside the usual
`temperature` and `ds18b20` tags, so a search or a planner can find the boards
that are rated for those media. The `air` tag belongs to the bare breakout, which
is the right part for ambient temperature.
