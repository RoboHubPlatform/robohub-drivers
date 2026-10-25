/**
 * @file led_matrix.h
 * @brief Driver - an LED matrix as a RoboHubDisplay.
 *
 * A matrix of LEDs driven by a shift register or a GPIO expander. The driver
 * implements the display contract over whatever the platform provides, so a
 * program that writes a pixel writes it the same way on any matrix.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/display.h"
#include "robohub/hal/gpio.h"

typedef struct {
    RoboHubDisplay interface; /* Must remain the first member. */
    RoboHubGPIO*   gpio;
    rh_u8          data_pin;
    rh_u8          clock_pin;
    rh_u8          latch_pin;
    rh_u8          width;
    rh_u8          height;
    rh_bool        ready;
} RoboHubLedMatrix;

RoboHubError robohub_led_matrix_init(RoboHubLedMatrix* display,
                                     RoboHubGPIO* gpio,
                                     rh_u8 data_pin,
                                     rh_u8 clock_pin,
                                     rh_u8 latch_pin,
                                     rh_u8 width,
                                     rh_u8 height);

#ifdef __cplusplus
}
#endif

#endif /* LED_MATRIX_H */
