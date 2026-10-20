/*
 * tone_pwm.c — a tone on a PWM channel.
 *
 * A square wave of the asked frequency is a duty of half at that frequency; the
 * peripheral makes the wave, and a note is two writes.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "tone_pwm.h"

#include <stddef.h>

static RoboHubError tone_op_stop(RoboHubTone* self);

static RoboHubError tone_op_init(RoboHubTone* self)
{
    RoboHubTonePwm* tone = (RoboHubTonePwm*)self->context;
    if (tone->pwm == NULL || tone->pwm->set_duty == NULL || tone->pwm->set_frequency == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    tone->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError tone_op_deinit(RoboHubTone* self)
{
    RoboHubTonePwm* tone = (RoboHubTonePwm*)self->context;
    tone->ready = RH_FALSE;
    if (tone->pwm != NULL && tone->pwm->stop != NULL) {
        return tone->pwm->stop(tone->pwm, tone->channel);
    }
    return ROBOHUB_OK;
}

static RoboHubError tone_op_play(RoboHubTone* self, rh_u32 frequency_hz)
{
    RoboHubTonePwm* tone = (RoboHubTonePwm*)self->context;
    if (!tone->ready) {
        return ROBOHUB_ERR_STATE;
    }
    if (frequency_hz == 0u) {
        return tone_op_stop(self);
    }
    RoboHubError err = tone->pwm->set_frequency(tone->pwm, frequency_hz);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    return tone->pwm->set_duty(tone->pwm, tone->channel, 0.5f);
}

static RoboHubError tone_op_stop(RoboHubTone* self)
{
    RoboHubTonePwm* tone = (RoboHubTonePwm*)self->context;
    if (!tone->ready || tone->pwm->stop == NULL) {
        return ROBOHUB_ERR_STATE;
    }
    return tone->pwm->stop(tone->pwm, tone->channel);
}

RoboHubError robohub_tone_pwm_init(RoboHubTonePwm* tone, RoboHubPWM* pwm, rh_u8 channel)
{
    if (tone == NULL || pwm == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    tone->pwm = pwm;
    tone->channel = channel;
    tone->ready = RH_FALSE;

    tone->interface.name = "buzzer";
    tone->interface.context = tone;
    tone->interface.init = tone_op_init;
    tone->interface.deinit = tone_op_deinit;
    tone->interface.play = tone_op_play;
    tone->interface.stop = tone_op_stop;

    return tone_op_init(&tone->interface);
}
