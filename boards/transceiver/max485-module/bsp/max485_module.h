/*
 * bsp.h — Support package for the MAX485 RS485 transceiver module.
 *
 * A MAX485 (or MAX3485, SN75176) is not a sensor and not a driver: it is the
 * transceiver that turns a UART into an RS485 bus. It has four logic pins — DI,
 * RO, DE and RE — of which DI and RO go to the UART's TX and RX and DE/RE go to
 * one GPIO the host drives: high to talk, low to listen. This module is the
 * carrier's support package for that wiring.
 *
 * Its one job is the join: it looks the UART and the direction pin's controller
 * up in the registry, binds them into a `RoboHubRS485` through robohub-common's
 * adapter, and registers that bus under a name a Modbus master or any other
 * RS485 protocol opens. A board that does not carry 485 natively gets one by
 * fitting this module and calling `robohub_max485_init` — which is what makes
 * 485 a dependency rather than a platform requirement.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_MAX485_MODULE_BSP_BSP_H
#define BOARDS_MAX485_MODULE_BSP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/rs485.h"
#include "robohub/hal/rs485_uart.h"

/* -------------------------------------------------------------------------- */
/*  Board identification                                                      */
/* -------------------------------------------------------------------------- */
#define RH_BOARD_MAX485_MODULE 1

/* -------------------------------------------------------------------------- */
/*  Module signals                                                            */
/* -------------------------------------------------------------------------- */
/* The logic side, as the carrier silkscreens it. A and B are the bus pair. */
#define RH_MAX485_MODULE_PIN_VCC 1
#define RH_MAX485_MODULE_PIN_GND 2
#define RH_MAX485_MODULE_PIN_DI  3  /* to the UART's TX */
#define RH_MAX485_MODULE_PIN_RO  4  /* to the UART's RX */
#define RH_MAX485_MODULE_PIN_DE  5  /* to a GPIO: drive high to transmit */
#define RH_MAX485_MODULE_PIN_RE  6  /* tied to DE: drive high to transmit */

/* -------------------------------------------------------------------------- */
/*  Defaults                                                                  */
/* -------------------------------------------------------------------------- */
/* A board names these differently, so a program passes its own. The bus name is
 * what a protocol opens; uart1 is the Pico's second link and the one a downstream
 * bus uses, and gpio0 is the general pin controller a platform registers. */
#define RH_MAX485_MODULE_NAME            "rs485_0"
#define RH_MAX485_MODULE_UART_NAME       "uart1"
#define RH_MAX485_MODULE_DE_GPIO_NAME    "gpio0"
#define RH_MAX485_MODULE_DE_PIN          20u
#define RH_MAX485_MODULE_TIMER_NAME      "system_timer"
#define RH_MAX485_MODULE_BAUD            9600u

/* -------------------------------------------------------------------------- */
/*  Configuration                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief What a program says about the module it fitted.
 *
 * The UART it is wired to, the pin the direction line is on, and how the line
 * is paced. Every field has a working default, so the ordinary wiring is an
 * empty configuration. `de_gpio` empty means no pin is driven — a bus whose
 * adapter has automatic direction.
 */
typedef struct {
    /** Registry name to publish the bus under. Empty means "rs485_0". */
    rh_name name;
    /** Registry name of the UART. Empty means "uart1". */
    rh_name uart;
    /** Registry name of the GPIO controller the direction pin is on. Empty
     *  means no pin is driven (automatic direction). */
    rh_name de_gpio;
    /** The direction pin. */
    rh_u32  de_pin;
    /** Whether driving high is transmit. False for an inverted enable. */
    rh_bool de_active_high;
    /** Registry name of the timer. Empty means "system_timer". */
    rh_name timer;
    /** The line speed. Zero means 9600, Modbus's default. */
    rh_u32  baud;
} RoboHubMax485Config;

/* -------------------------------------------------------------------------- */
/*  Board functions                                                           */
/* -------------------------------------------------------------------------- */

/** The configuration the module is used with when a program says nothing. */
const RoboHubMax485Config* robohub_max485_default_config(void);

/** The carrier has nothing to bring up on its own; the hook exists so fitting a
 *  module reads the same as bringing a board up. */
void robohub_max485_bsp_init(void);

/**
 * @brief Bind the transceiver's UART and direction pin as an RS485 bus.
 *
 * Looks the UART, the direction GPIO and the timer up in the registry, fills in
 * `bus` through robohub-common's adapter, brings it up at the configured speed
 * and registers it under its name. Returns `ROBOHUB_ERR_NOT_FOUND` when a named
 * peripheral is not registered — the platform did not bring it up.
 *
 * The caller owns `bus`; it must outlive the program's use of the bus.
 */
RoboHubError robohub_max485_init(RoboHubRS485Uart* bus, const RoboHubMax485Config* config);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_MAX485_MODULE_BSP_BSP_H */
