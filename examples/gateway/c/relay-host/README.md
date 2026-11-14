# gateway/c/relay-host

A relay on a host: the whole middle of a RoboHub tree without a board.

It is the board relay (`examples/gateway/c/relay`) built for `robohub-linux`. It
hears boards on a downstream tty and dials the gateway above it over TCP. It
holds no key of a board's: each board's hello and sealed frames go up unchanged
inside `RELAYED`, sealed under the relay's own session, and what the gateway
sends a board comes back the same way and is written to the board's tty. The
gateway authenticates each board with the board's own key, so the relay can drop
or delay a frame and cannot forge or change one. Because it runs on a host, the
tree — master, relay, boards — can be run and tested on one machine.

```sh
rho build -target native .
```

## Environment

| Variable | Meaning |
|---|---|
| `RHO_UART1` | the downstream tty the boards are wired to |
| `RHO_PARENT_HOST`, `RHO_PARENT_PORT` | the gateway above, a `robohub-gateway` with `UPLINK_PORT` set |
| `RH_SECURE_UID`, `RH_SECURE_KEY_FILE` | the relay's id (16 hex digits) and a file holding its key (32 hex digits), as every host node has them; a build with `robohub_secret.h` carries them instead. Without them its links are plain and a parent hears nothing from it |
| `CONFIG_RH_AREA_ID` (build) | the area this relay speaks for |

## The whole tree on one machine

`robohub-gateway/tools/relay-smoke.py` runs the master, this relay and sealed
boards (a pseudo-terminal) together and checks that a board behind the relay
gets a session and its reading reaches the master as the board's, that a command
reaches the board and its answer comes back, that a relay cannot forge a
reading, that a plain command written to the relay's console (a board's USB
CDC) reaches no board, and that after the master restarts the relay dials again,
announces again, and the board is heard. It builds this relay when it is not
built.

```sh
cd ../robohub-gateway
python3 tools/relay-smoke.py
```

## What it is, and what it is not

It is a **relay**: it measures nothing, acts only on what its parent sends under
its session — `RELAYED` for a board, and a diagnostic about itself (node 0 of
its area) — and dials its parent again when the parent goes away. Its console is
a parent link only when it is sealed; unsealed, a frame written to it is
dropped. Its downstream read is the UART HAL's, one byte at a
time — the contract transfers the bytes it is asked for and reports whether it
did, so a byte is the unit that carries a count without a platform-specific "how
much is waiting". That is the one change that makes the board relay's program a
host program.
