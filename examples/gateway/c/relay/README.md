# gateway/c/relay

A Pico 2W that gateways without measuring: a pure hop of a RoboHub tree.

```sh
rho build -target pico2w .
```

## What it is

The edge's stack (`examples/gateway/c/repeater`) minus the sensor:

- Frames that arrive on `uart1` are fed to a `robohub-transport`, which publishes
  the `Telemetry` event on the event bus.
- `robohub-telemetry` bridges that event to the upstream `robohub-transport`,
  whose bytes go out the USB CDC.

Nothing forwards a board by hand. A reading names its device (an area and a
node), so the bus needs no source table: every downstream frame publishes the
same event, and the bridge that carries one carries all.

A relay differs from an edge in what it does not have: no sensor, no sample task,
and so no reading of its own. Everything a host reads through it came from a
board behind it, which is what makes it a hop rather than a source.

## Wiring

| Pico 2W | Direction | Downstream board |
|---|---|---|
| GP4 (UART1 TX) | out | board RX |
| GP5 (UART1 RX) | in | board TX |
| GND | — | GND |

A downstream board runs the node firmware
(`examples/temperature/ds18b20/c/telemetry`) with its own `CONFIG_RH_AREA_ID` and
`CONFIG_RH_NODE_ID`. It speaks the same `link` schema and the same
`robohub-transport` framing; the only difference is that its link is a UART
rather than USB CDC.

## Chaining

A relay's upstream is its USB CDC, so what is above it is whatever reads that
link — a host running `robohub-gateway`, or another relay's `uart1`.

With `CONFIG_RH_NET` it also dials the gateway above it over TCP: it joins the
network, connects to `RH_PARENT_HOST:RH_PARENT_PORT`, sends an `ANNOUNCE`
naming the area it speaks for, and re-frames every reading it heard as an
uplink `READING` on that socket. Its parent therefore learns who it is and
which area it owns, and lists every board behind it — a relay is a hop of the
tree, not just a USB stream.

The parent's address is a build or deployment value, not a committed one:

```sh
rho build -target pico2w -DRH_PARENT_HOST=10.0.0.5 -DRH_PARENT_PORT=5001 .
```

A relay that cannot reach its parent is not a broken relay: the uplink is best
effort, and the boards behind it still stream to whatever reads the USB CDC.

## Routing down

A command or a diagnostic the parent sends for a board **behind** the relay is
routed down the downstream link with the parent's transaction id intact, so the
board answers that transaction and the answer is carried back up the uplink —
which is how the gateway that asked matches the answer to what it routed.

A relay answers for **itself** as node 0 of its area: a diagnostic with node 0 is
answered by the relay (its clock and what the registry holds), and one for any
other node goes down to the board that is. A relay answering for a board would be
answering about a device it is not.

## Notes

- One downstream link today, on `uart1` (GP4/GP5). A second UART or an RS-485
  transceiver is another `robohub-transport` fed the same way; RS-485 needs the
  direction pin and the `rs485` HAL, whose Pico implementation is still a stub.
- The uplink's re-framing is the one place the levels differ, and it is one
  function (`robohub_uplink_reframe`); running the relay on a host — and so
  testing the dialer end to end — is `examples/gateway/c/uplink-host`, on
  `robohub-linux`.
