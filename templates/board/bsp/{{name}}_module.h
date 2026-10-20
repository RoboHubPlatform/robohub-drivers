/*
 * bsp.h — Support package for the {{name}} eval board.
 *
 * The board's signals and the defaults it is reached with. It is the driver IC's
 * counterpart of a platform board: the platform says which pins a bus is on, and
 * this says which of them the module is wired to. The BSP resolves the bus from
 * the registry and hands it to the driver, so an application fits the board
 * without repeating those facts.
 *
 * A part reached by another wire — a GPIO, an ADC, a PWM — replaces the bus
 * below with that HAL, and nothing above it changes.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_{{name}}_MODULE_BSP_H
#define BOARDS_{{name}}_MODULE_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "{{name}}.h"
#include "robohub/hal/bus.h"

/* Board identification. */
#define RH_BOARD_{{name}}_MODULE 1

/* The wires the carrier exposes, as it silkscreens them. */
#define RH_{{name}}_MODULE_PIN_VCC 1
#define RH_{{name}}_MODULE_PIN_GND 2

/* The defaults it is reached with. A board where these differ passes its own. */
#define RH_{{name}}_MODULE_BUS_NAME "i2c0"
#define RH_{{name}}_MODULE_ADDRESS  0x00u
#define RH_{{name}}_MODULE_SPEED_HZ 400000u

/** What a program says about the board it fitted. */
typedef struct {
    /** Registry name of the controller. Empty means "i2c0". */
    rh_name i2c;
    /** The part's 7-bit address. Zero means 0x00. */
    rh_u8   address;
    /** Bus speed in hertz. Zero means 400000. */
    rh_u32  speed_hz;
} RoboHub{{name}}ModuleConfig;

/** The configuration the board is used with when a program says nothing. */
const RoboHub{{name}}ModuleConfig* robohub_{{name}}_module_default_config(void);

/** The carrier has nothing to bring up on its own; the hook exists for parity. */
void robohub_{{name}}_module_bsp_init(void);

/** Resolve the bus from the registry, bind it, and bring the part up. */
RoboHubError robohub_{{name}}_module_init(RoboHub{{name}}Device* dev,
                                          const RoboHub{{name}}ModuleConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_{{name}}_MODULE_BSP_H */
