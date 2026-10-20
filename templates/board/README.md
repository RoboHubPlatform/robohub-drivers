# {{name}} eval board

An eval board carrying a {{description}}.

It is the driver-IC counterpart of a platform board: the platform says which pins
a bus is on, and this says which of them the module is wired to. The BSP resolves
the bus from the registry and hands it to the driver, so the same module is
fitted to any board.

## Signals

| Signal | Meaning |
|--------|---------|
| VCC | Supply |
| GND | Ground |

## Driver

`robohub-driver-{{name}}` — the platform-agnostic driver under
`drivers/{{category}}/{{name}}`. Selecting `RH_BOARD_{{name}}_MODULE` brings it
in.
