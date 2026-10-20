/**
 * @file gpio_switch.h
 * @brief Driver - an on/off output on a GPIO, as a RoboHubSwitch.
 *
 * A pin that is high or low is the simplest thing a program can drive: a relay,
 * an active buzzer, an indicator, a self-flashing LED whose effect is switched
 * on and off as a whole. The switch is the interface, so a caller asks for the
 * thing to be on and not for a pin to be high.
 *
 * `active_high` is the wiring: a part whose input is pulled high to act is
 * active-high, one that is pulled low to act is active-low. The interface is in
 * terms of the switch being on, so the inversion is here and a caller never has
 * to think in it.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef GPIO_SWITCH_H
#define GPIO_SWITCH_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/gpio.h"
#include "robohub/devices/switch.h"

/** One output on one GPIO pin. */
typedef struct {
    RoboHubSwitch interface; /* Must remain the first member. */
    RoboHubGPIO*  gpio;
    rh_u32        pin;
    rh_bool       active_high;
    rh_bool       ready;
} RoboHubGpioSwitch;

/**
 * @brief Bind an output to a GPIO pin.
 *
 * Brings the pin up as an output and leaves it off.
 */
RoboHubError robohub_gpio_switch_init(RoboHubGpioSwitch* sw,
                                      RoboHubGPIO* gpio,
                                      rh_u32 pin,
                                      rh_bool active_high);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_SWITCH_H */
