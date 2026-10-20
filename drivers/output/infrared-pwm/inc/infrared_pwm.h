/**
 * @file infrared_pwm.h
 * @brief Driver - an infrared LED on a PWM channel, as a RoboHubInfrared.
 *
 * An IR LED sends a code as a carrier that is gated on and off: a mark is the
 * carrier for some microseconds, a space is silence for some. The carrier is a
 * PWM channel at 38 kHz; the pattern is what this drives, and the protocol that
 * builds one sits above it.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INFRARED_PWM_H
#define INFRARED_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/pwm.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/infrared.h"

/** The carrier most parts use, in hertz. */
#define ROBOHUB_INFRARED_CARRIER_HZ 38000u

typedef struct {
    RoboHubInfrared interface; /* Must remain the first member. */
    RoboHubPWM*     pwm;
    RoboHubTimer*   timer;
    rh_u8           channel;
    rh_u32          carrier;
    rh_bool         ready;
} RoboHubInfraredPwm;

/**
 * @brief Bind an infrared transmitter to a PWM channel and a timer.
 *
 * The timer is what times the marks and spaces; without one the driver cannot
 * send a sequence. The carrier defaults to 38 kHz.
 */
RoboHubError robohub_infrared_pwm_init(RoboHubInfraredPwm* ir,
                                       RoboHubPWM* pwm,
                                       RoboHubTimer* timer,
                                       rh_u8 channel);

#ifdef __cplusplus
}
#endif

#endif /* INFRARED_PWM_H */
