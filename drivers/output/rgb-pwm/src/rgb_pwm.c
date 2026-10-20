/*
 * rgb_pwm.c — a colour LED on PWM channels.
 *
 * A channel's brightness is its duty: a byte of colour is that fraction of full,
 * and a channel the part does not have is one the write skips.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "rgb_pwm.h"

#include <stddef.h>

static RoboHubError rgb_op_init(RoboHubRgb* self)
{
    RoboHubRgbPwm* rgb = (RoboHubRgbPwm*)self->context;
    if (rgb->pwm == NULL || rgb->pwm->set_duty == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rgb->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError rgb_op_deinit(RoboHubRgb* self)
{
    RoboHubRgbPwm* rgb = (RoboHubRgbPwm*)self->context;
    rgb->ready = RH_FALSE;
    for (rh_u8 i = 0; i < 3u; i++) {
        if (rgb->channel[i] != ROBOHUB_RGB_NO_CHANNEL && rgb->pwm->stop != NULL) {
            (void)rgb->pwm->stop(rgb->pwm, rgb->channel[i]);
        }
    }
    return ROBOHUB_OK;
}

static rh_u8 rgb_op_channels(RoboHubRgb* self)
{
    return ((RoboHubRgbPwm*)self->context)->count;
}

static RoboHubError rgb_op_set(RoboHubRgb* self, rh_u8 red, rh_u8 green, rh_u8 blue)
{
    RoboHubRgbPwm* rgb = (RoboHubRgbPwm*)self->context;
    if (!rgb->ready) {
        return ROBOHUB_ERR_STATE;
    }
    rh_u8 value[3] = { red, green, blue };
    for (rh_u8 i = 0; i < 3u; i++) {
        if (rgb->channel[i] == ROBOHUB_RGB_NO_CHANNEL) {
            continue;
        }
        RoboHubError err = rgb->pwm->set_duty(rgb->pwm, rgb->channel[i], (rh_f32)value[i] / 255.0f);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
    }
    return ROBOHUB_OK;
}

RoboHubError robohub_rgb_pwm_init(RoboHubRgbPwm* rgb, RoboHubPWM* pwm,
                                  rh_u8 red, rh_u8 green, rh_u8 blue)
{
    if (rgb == NULL || pwm == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rgb->pwm = pwm;
    rgb->channel[0] = red;
    rgb->channel[1] = green;
    rgb->channel[2] = blue;
    rgb->count = 0u;
    for (rh_u8 i = 0; i < 3u; i++) {
        if (rgb->channel[i] != ROBOHUB_RGB_NO_CHANNEL) {
            rgb->count++;
        }
    }
    rgb->ready = RH_FALSE;

    rgb->interface.name = "rgb";
    rgb->interface.context = rgb;
    rgb->interface.init = rgb_op_init;
    rgb->interface.deinit = rgb_op_deinit;
    rgb->interface.channels = rgb_op_channels;
    rgb->interface.set = rgb_op_set;

    return rgb_op_init(&rgb->interface);
}
