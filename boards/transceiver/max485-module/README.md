# MAX485 RS485 transceiver module

An RS485 transceiver carrier built on a MAX485 (or MAX3485, SN75176). It is not
a sensor and not a driver: it is the physical layer that turns a UART into a
half-duplex RS485 bus, so a board that has no RS485 of its own gets one by
fitting this module.

Where a platform board says which MCU pin its peripherals reach, this says which
UART the transceiver is wired to and which pin drives its direction line. The
join is robohub-common's RS485 adapter, which turns a `RoboHubUART` and a
direction `RoboHubGPIO` into a `RoboHubRS485`; this module's BSP is the
registry-specific layer that looks the two up by name and publishes the bus.

## Signals

| Signal | Meaning |
|--------|---------|
| VCC | Supply, 5 V (or 3.3 V on a 3.3 V part such as the MAX3485) |
| GND | Ground |
| DI | Driver input — the UART's TX |
| RO | Receiver output — the UART's RX |
| DE | Driver enable — a GPIO, driven high to transmit |
| RE | Receiver enable — usually tied to DE, so low receives |
| A | Bus, non-inverting |
| B | Bus, inverting |

The board's 120 ohm termination belongs at the two ends of the bus, not on every
node. DE and RE are normally wired together: driving the pin high both enables
the driver and disables the receiver, which is exactly half-duplex.

## Defaults

| Setting | Value |
|---------|-------|
| Bus name | `rs485_0` |
| UART | `uart1` |
| Direction GPIO | `gpio0`, pin 20 |
| Transmit level | high |
| Timer | `system_timer` |
| Baud | 9600 (Modbus's default) |

`bsp.h` names them, and a board where they differ passes its own configuration.
An empty `de_gpio` means no pin is driven — a USB-RS485 adapter with automatic
direction, which presents the same bus.

## Using it

```c
RoboHubRS485Uart bus;
robohub_max485_init(&bus, NULL);          /* the defaults above */

RoboHubRS485* link = robohub_get_rs485("rs485_0");
```

A Modbus master, or any other RS485 protocol, opens the bus by name and never
hears about the transceiver. `set_direction` drives the pin; `write` does the
direction change and the inter-character wait around the UART write for you, so
the last byte is not truncated.

## Platform requirement

The adapter needs a UART and, when the direction is not automatic, a GPIO that
can be driven. On the Pico 2W, `robohub-pico` registers `uart1` and a general
`gpio0`; on a Linux host, `RHO_UART1` names a tty (a USB-RS485 adapter) and no
direction pin is used. A USB-RS485 adapter is the host equivalent of this
module: the same Modbus stack runs over it unchanged.
