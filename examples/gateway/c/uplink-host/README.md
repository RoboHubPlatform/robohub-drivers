# gateway/c/uplink-host

A relay's uplink, on a host.

The gateway-only mode of a relay, built for `robohub-linux` rather than a board:
it brings the platform up, connects to the gateway above it, announces the area
it speaks for, and sends a reading up. It is the smallest program that exercises
the uplink end to end, and the one that runs where there is no board.

## Build and run

```sh
rho build -target native .
./build/output/uplink_host <host> <port>
```

`<host> <port>` is the gateway above — a `robohub-gateway` with `UPLINK_PORT`
set. On the gateway, `GET /api/v1/nodes` then lists the reading this sent, and
`GET /api/v1/diagnostics` reports it as a child.

```sh
# on the gateway
UPLINK_PORT=5001 REST_PORT=8080 robohub-gateway &

# here
./build/output/uplink_host 127.0.0.1 5001
curl -s localhost:8080/api/v1/nodes
# [{"area_id":9,"node_id":6,"temperature_c":18.25,"status":"OK","last_updated_ms":5555}]
```

## What it is, and what it is not

It is the **uplink** half of a relay: the socket, the announce, and the
re-framing of a reading into the uplink's shape (`robohub_uplink_reframe`). A
real relay hears its readings on a downstream link and forwards each one; this
one is handed a reading directly, because what it demonstrates is the uplink, not
the sensor. The downstream half is the board relay
(`examples/gateway/c/relay`).

It runs on a host because `robohub-linux` supplies the platform: the console, the
clock, a serial port and sockets, over POSIX. That is what makes a gateway-only
mode a program on a server or in a container instead of a board.

## Note

A control routed down to this relay is not yet forwarded to a board behind it.
The master sends one when a command names a device in this relay's area; passing
it on down the downstream link is the next step, and it is the same code the
board relay needs.
