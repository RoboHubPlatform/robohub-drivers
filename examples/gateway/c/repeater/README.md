# gateway/c/repeater

A Pico 2W that measures its own temperature and repeats the boards wired to it.

```sh
rho build -target pico2w .
```

## What it is

The same stack as `examples/temperature/ds18b20/c/telemetry`, with one more
link:

- The local DS18B20 publishes `Telemetry` (`0x0201`) on the event bus, as before.
- Frames that arrive on `uart1` are fed to a `robohub-transport`, which publishes
  the same `Telemetry` event on the same bus.
- `robohub-telemetry` bridges `0x0201` to the upstream `robohub-transport`,
  whose bytes go out the USB CDC.

Nothing forwards a board by hand. A reading names its board (`node_id`), so the
bus needs no source table: the local reading and every downstream frame are the
same event, and the bridge that carries one carries all. The Pico is a node and
a gateway at once.

## Wiring

| Pico 2W | Direction | Downstream board |
|---|---|---|
| GP4 (UART1 TX) | out | board RX |
| GP5 (UART1 RX) | in | board TX |
| GND | — | GND |

A downstream board runs the node firmware with its own `CONFIG_RH_NODE_ID`
(`examples/temperature/ds18b20/c/telemetry`), set to a value distinct from this
Pico's. It speaks the same `Telemetry` schema and the same `robohub-transport`
framing; the only difference is that its link is a UART rather than USB CDC.

## Upstream and the host

- **Repeater:** with a host on the USB CDC, run `robohub-gateway` with
  `SERIAL_PORT=/dev/ttyACM0`. `GET /api/v1/nodes` then lists this Pico and every
  board behind it.
- **Main gateway:** with no host, it streams upstream to whatever reads the
  USB CDC; a Wi-Fi sink is the `robohub-telemetry` socket sink, added the way
  the node example adds it.

## Notes

- One downstream link today, on `uart1` (GP4/GP5). A second UART or an RS-485
  transceiver is another `robohub-transport` fed the same way; RS-485 needs the
  direction pin and the `rs485` HAL, whose Pico implementation is still a stub.
- A sampling-rate command (`0x0301`) is answered for this Pico. Commands for a
  board behind it are not yet routed by node id.
