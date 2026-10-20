/**
 * @file button_gpio.h
 * @brief Driver - a discrete contact on a GPIO, as a RoboHubButton.
 *
 * A key switch, a tilt switch, a vibration switch, a knock sensor: each is a
 * contact that is closed or open, wired to a pin, and each is a `RoboHubButton`.
 * What differs between them is only the level a closed contact pulls the pin to,
 * and that is `active_low`.
 *
 * It is one driver rather than one per part because the parts have no protocol:
 * none of them has an address or a register, and a program that reads a tilt and
 * one that reads a knock are the same program. A board that carries one of them
 * wires this driver to its pin, and the interface a program sees is the part's
 * behaviour and not its name.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BUTTON_GPIO_H
#define BUTTON_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/gpio.h"
#include "robohub/drivers/button.h"

/** One contact on one GPIO pin. */
typedef struct {
    RoboHubButton interface; /* Must remain the first member. */
    RoboHubGPIO*  gpio;
    rh_u32        pin;
    rh_bool       active_low; /* Whether a closed contact pulls the pin low. */
    rh_bool       ready;
} RoboHubButtonGpio;

/**
 * @brief Bind a contact to a GPIO pin.
 *
 * Brings the pin up as an input. A pin with no external pull needs one on the
 * board; the HAL has no pull here, so the carrier provides it.
 */
RoboHubError robohub_button_gpio_init(RoboHubButtonGpio* button,
                                      RoboHubGPIO* gpio,
                                      rh_u32 pin,
                                      rh_bool active_low);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_GPIO_H */
