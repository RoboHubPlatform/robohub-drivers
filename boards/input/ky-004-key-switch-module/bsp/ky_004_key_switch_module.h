/*
 * bsp.h — Support package for the ky-004-key-switch eval board.
 *
 * The board's wiring and the defaults it is reached with. It resolves the
 * interface from the registry and hands it to the driver, so an application fits
 * the board without repeating which pin or channel it is on. The wiring is a
 * macro so a board overrides it from the build.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_KY_004_KEY_SWITCH_MODULE_H
#define BOARDS_KY_004_KEY_SWITCH_MODULE_H

#ifdef __cplusplus
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/drivers/input/button_gpio.h"

#define RH_BOARD_KY_004_KEY_SWITCH 1

/* The wiring, as the carrier exposes it. */
#define RH_KY_004_KEY_SWITCH_GPIO_NAME "gpio0"
#define RH_KY_004_KEY_SWITCH_PIN 2u
#define RH_KY_004_KEY_SWITCH_ACTIVE_LOW 1

/** The carrier has nothing to bring up on its own. */
void robohub_ky_004_key_switch_module_bsp_init(void);

/** Resolve the interface from the registry and bind the driver. */
RoboHubError robohub_ky_004_key_switch_module_init(RoboHubButtonGpio* dev);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_KY_004_KEY_SWITCH_MODULE_H */
