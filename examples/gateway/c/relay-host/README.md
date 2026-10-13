# gateway/c/relay-host

A relay on a host: the whole middle of a RoboHub tree without a board.

It is the board relay (`examples/gateway/c/relay`) built for `robohub-linux`. It
hears boards on a downstream tty, dials the gateway above it over TCP, forwards
their readings up, and routes a command or a diagnostic addressed to a board
behind it **down** and the answer **back up**. Because it runs on a host, the
tree — master, relay, board — can be run and tested on one machine.

```sh
rho build -target native .
```

## Environment

| Variable | Meaning |
|---|---|
| `RHO_UART1` | the downstream tty the boards are wired to |
| `RHO_PARENT_HOST`, `RHO_PARENT_PORT` | the gateway above, a `robohub-gateway` with `UPLINK_PORT` set |
| `CONFIG_RH_AREA_ID` (build) | the area this relay speaks for |

## The whole tree on one machine

`robohub-gateway/tools/relay-smoke.py` runs the master, this relay and a board
(a pseudo-terminal) together and checks both directions:

- the relay dials the master and announces its area;
- the board's reading reaches the master through the relay;
- a command the master sends for that board is routed by the relay to the
  board's link — the board hears it naming its own device — and the board's
  answer is carried back to the master.

```sh
cd ../robohub-gateway
python3 tools/relay-smoke.py
```

## What it is, and what it is not

It is a **relay**: it measures nothing and answers a diagnostic only about
itself (node 0 of its area). Its downstream read is the UART HAL's, one byte at a
time — the contract transfers the bytes it is asked for and reports whether it
did, so a byte is the unit that carries a count without a platform-specific "how
much is waiting". That is the one change that makes the board relay's program a
host program.
