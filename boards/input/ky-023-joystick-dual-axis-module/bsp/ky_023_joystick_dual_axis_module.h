/*
 * bsp.h — Support package for the ky-023-joystick-dual-axis eval board.
 *
 * A joystick is three devices: the two axes and the switch. A board may carry
 * more than one, so the support package binds each of them, and the wiring is
 * a macro so a board overrides it from the build.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_KY_023_JOYSTICK_DUAL_AXIS_MODULE_H
#define BOARDS_KY_023_JOYSTICK_DUAL_AXIS_MODULE_H

#ifdef __cplusplus
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/drivers/input/analog_adc.h"
#include "robohub/drivers/input/button_gpio.h"

#define RH_BOARD_KY_023_JOYSTICK_DUAL_AXIS_MODULE 1

/* The wiring, as the carrier exposes it. */
#define RH_KY_023_JOYSTICK_ADC_NAME "adc0"
#define RH_KY_023_JOYSTICK_X_CHANNEL 0u
#define RH_KY_023_JOYSTICK_Y_CHANNEL 1u
#define RH_KY_023_JOYSTICK_GPIO_NAME "gpio0"
#define RH_KY_023_JOYSTICK_SWITCH_PIN 2u
#define RH_KY_023_JOYSTICK_SWITCH_ACTIVE_LOW 1

/** The carrier has nothing to bring up on its own. */
void robohub_ky_023_joystick_dual_axis_module_bsp_init(void);

/** Bind the horizontal axis. */
RoboHubError robohub_ky_023_joystick_dual_axis_module_init_x(RoboHubAnalogAdc* axis);

/** Bind the vertical axis. */
RoboHubError robohub_ky_023_joystick_dual_axis_module_init_y(RoboHubAnalogAdc* axis);

/** Bind the push switch. */
RoboHubError robohub_ky_023_joystick_dual_axis_module_init_switch(RoboHubButtonGpio* press);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_KY_023_JOYSTICK_DUAL_AXIS_MODULE_H */
