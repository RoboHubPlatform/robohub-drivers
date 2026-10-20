/**
 * @file encoder_gpio.h
 * @brief Driver - a rotary encoder on two GPIOs, as a RoboHubEncoder.
 *
 * A knob that turns: two signals whose order says which way, and a pulse for
 * each detent. The driver watches the two pins and counts the detents that
 * passed, so the count is the knob's and not the reader's.
 *
 * The reading is by polling: the driver looks at the pins when it is asked, so
 * a knob turned faster than it is read loses steps. A part that must not lose
 * any wants the edges themselves, which is what an interrupt-driven driver
 * would take; this one is for the knob a person turns slowly.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef ENCODER_GPIO_H
#define ENCODER_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/gpio.h"
#include "robohub/drivers/encoder.h"

/** One encoder on two pins. */
typedef struct {
    RoboHubEncoder interface; /* Must remain the first member. */
    RoboHubGPIO*  gpio;
    rh_u32        pin_a;
    rh_u32        pin_b;
    rh_bool       invert;   /* The pins are the other way round. */
    rh_i32        steps;    /* Detents passed since the last read. */
    rh_u8         last_state;
    rh_bool       ready;
} RoboHubEncoderGpio;

/**
 * @brief Bind an encoder to two GPIO pins.
 *
 * `invert` is the wiring: true when the two pins are the other way round, which
 * makes the knob turn the other way.
 */
RoboHubError robohub_encoder_gpio_init(RoboHubEncoderGpio* encoder,
                                       RoboHubGPIO* gpio,
                                       rh_u32 pin_a,
                                       rh_u32 pin_b,
                                       rh_bool invert);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_GPIO_H */
