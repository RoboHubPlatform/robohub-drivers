# gateway/c/relay

A Pico 2W that gateways without measuring: a pure hop of a RoboHub tree.

```sh
rho build -target pico2w .
```

## What it is

A relay holds no key of any board's. It carries what each board behind it sends
— the board's hello, and every frame the board seals under its own session with
the gateway above — **unchanged**, inside a `RELAYED` message (`0x0013`) sealed
under the relay's own session with its parent:

```text
port(u8) | inner_msg_id(u16) | inner_tx_id(u16) | inner_payload
```

The parent opens the inner frame with the board's own key and keeps the reading
under the board's id. What the parent sends a board — its session, and every
frame sealed for it — comes down the same way, and the relay writes the inner
frame to the board's port (`uart1` is port 1). So the relay can drop or delay a
board's frame, and cannot forge, change or redirect one: it does not parse a
reading, nor build one under its own identity.

A board's plain frame is not carried — the parent refuses it, and the relay
cannot vouch for it. Frames from `uart1` are taken by a frame hook
(`robohub_transport_on_frame`, in robohub-transport) before the event bus, so a
board's frame is never acted on by the relay.

## Identity

The relay's id and key (`RH_DEVICE_UID`, `RH_DEVICE_KEY`) are written into its
build by Mesh as `robohub_secret.h`, which no repository tracks. In the node
manifest it is a `relay`: an in-mesh relay carries the boards placed in its own
area; one for a linked mesh, the areas listed for it. Unprovisioned, its links
are plain and a parent hears nothing from it.

## Wiring

| Pico 2W | Direction | Downstream board |
|---|---|---|
| GP4 (UART1 TX) | out | board RX |
| GP5 (UART1 RX) | in | board TX |
| GND | — | GND |

A downstream board runs node firmware with its own id and key, sealed
(`RH_SECURE_PROVISIONED`); it speaks the same `robohub-transport` framing on a
UART as on USB CDC.

## Its parent

With `CONFIG_RH_NET` it joins the network and dials the gateway above it,
`RH_PARENT_HOST:RH_PARENT_PORT`, opens a session, and announces the area it
speaks for. When the parent goes away it dials again every two seconds, asks for
a new session, and announces again as soon as the session is new — a parent that
restarted has forgotten it. The boards behind it keep their own sessions.

```sh
rho build -target pico2w -DRH_PARENT_HOST=10.0.0.5 -DRH_PARENT_PORT=5001 .
```

Without the socket its parent is whatever reads the USB CDC — a host gateway —
and the USB CDC is sealed under a session of its own. Unprovisioned, the USB CDC
carries nothing up and anything written to it is dropped: a frame on the USB port
never reaches a board.

## What it acts on

Only what comes from its parent under its session: `RELAYED` for a board on one
of its ports, and a diagnostic about itself — node 0 of its area — answered with
its clock and what the registry holds. A command or diagnostic for a board comes
inside `RELAYED`, sealed for the board.

## Notes

- One downstream link today, on `uart1` (GP4/GP5), port 1. A second UART or an
  RS-485 transceiver is another `robohub-transport` with the same hook and its
  own port number.
- The same program on a host is `examples/gateway/c/relay-host`, which is how
  the tree is tested end to end (`robohub-gateway/tools/relay-smoke.py`).
