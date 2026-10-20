/**
 * @file rgb_pwm.h
 * @brief Driver - a colour LED on PWM channels, as a RoboHubRgb.
 *
 * An RGB LED is three LEDs in one package, and each channel's brightness is its
 * duty. A two-colour LED is the same with two, which is why the channels are
 * given rather than assumed: a part with no blue channel is a part with two.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef RGB_PWM_H
#define RGB_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/pwm.h"
#include "robohub/drivers/rgb.h"

/** The channel value that means "this part has no such colour". */
#define ROBOHUB_RGB_NO_CHANNEL 0xFFu

typedef struct {
    RoboHubRgb interface; /* Must remain the first member. */
    RoboHubPWM* pwm;
    rh_u8       channel[3]; /* red, green, blue; NO_CHANNEL when absent */
    rh_u8       count;
    rh_bool     ready;
} RoboHubRgbPwm;

/**
 * @brief Bind a colour output to PWM channels.
 *
 * Each channel is a PWM output, or `ROBOHUB_RGB_NO_CHANNEL` for a colour the
 * part does not have — a two-colour LED passes one of them, an RGB LED passes
 * three.
 */
RoboHubError robohub_rgb_pwm_init(RoboHubRgbPwm* rgb, RoboHubPWM* pwm,
                                  rh_u8 red, rh_u8 green, rh_u8 blue);

#ifdef __cplusplus
}
#endif

#endif /* RGB_PWM_H */
