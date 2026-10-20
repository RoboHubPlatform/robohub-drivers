/**
 * @file tone_pwm.h
 * @brief Driver - a tone on a PWM channel, as a RoboHubTone.
 *
 * A passive buzzer, a small speaker: a part that sounds at a frequency. A PWM
 * channel drives it at that frequency and a duty of half, which is a square wave
 * the part turns into a note.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef TONE_PWM_H
#define TONE_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/pwm.h"
#include "robohub/drivers/tone.h"

typedef struct {
    RoboHubTone interface; /* Must remain the first member. */
    RoboHubPWM* pwm;
    rh_u8       channel;
    rh_bool     ready;
} RoboHubTonePwm;

/** Bind a tone output to a PWM channel. */
RoboHubError robohub_tone_pwm_init(RoboHubTonePwm* tone, RoboHubPWM* pwm, rh_u8 channel);

#ifdef __cplusplus
}
#endif

#endif /* TONE_PWM_H */
