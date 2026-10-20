/*
 * bsp.h — Support package for the ky-034-automatic-flashing-color-led eval board.
 *
 * The board's wiring and the defaults it is reached with. It resolves the
 * interface from the registry and hands it to the driver, so an application fits
 * the board without repeating which pin or channel it is on. The wiring is a
 * macro so a board overrides it from the build.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_KY_034_AUTOMATIC_FLASHING_COLOR_LED_MODULE_H
#define BOARDS_KY_034_AUTOMATIC_FLASHING_COLOR_LED_MODULE_H

#ifdef __cplusplus
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/drivers/output/gpio_switch.h"

#define RH_BOARD_KY_034_AUTOMATIC_FLASHING_COLOR_LED 1

/* The wiring, as the carrier exposes it. */
#define RH_KY_034_AUTOMATIC_FLASHING_COLOR_LED_GPIO_NAME "gpio0"
#define RH_KY_034_AUTOMATIC_FLASHING_COLOR_LED_PIN 2u
#define RH_KY_034_AUTOMATIC_FLASHING_COLOR_LED_ACTIVE_HIGH 1

/** The carrier has nothing to bring up on its own. */
void robohub_ky_034_automatic_flashing_color_led_module_bsp_init(void);

/** Resolve the interface from the registry and bind the driver. */
RoboHubError robohub_ky_034_automatic_flashing_color_led_module_init(RoboHubGpioSwitch* dev);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_KY_034_AUTOMATIC_FLASHING_COLOR_LED_MODULE_H */
