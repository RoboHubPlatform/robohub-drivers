/*
 * pulse_adc.c — a pulse in an analog signal.
 *
 * A rhythm is what crosses the threshold, and how long it is between crossings
 * is the rate. The first crossing has no interval to speak of, so the rate is
 * only meaningful once there is a rhythm to measure.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pulse_adc.h"

#include <stddef.h>

static RoboHubError pulse_op_init(RoboHubPulse* self)
{
    RoboHubPulseAdc* pulse = (RoboHubPulseAdc*)self->context;
    if (pulse->adc == NULL || pulse->adc->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    pulse->above = RH_FALSE;
    pulse->last_beat_ms = 0u;
    pulse->beats_per_minute = 0u;
    pulse->beat = RH_FALSE;
    pulse->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError pulse_op_deinit(RoboHubPulse* self)
{
    ((RoboHubPulseAdc*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError pulse_op_read(RoboHubPulse* self, rh_u16* beats_per_minute, rh_bool* beat)
{
    RoboHubPulseAdc* pulse = (RoboHubPulseAdc*)self->context;
    if (beats_per_minute == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!pulse->ready) {
        return ROBOHUB_ERR_STATE;
    }

    rh_f32 level = 0.0f;
    RoboHubError err = pulse->adc->read(pulse->adc, pulse->channel, &level);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_bool over = level > pulse->threshold ? RH_TRUE : RH_FALSE;
    if (over && !pulse->above) {
        /* A crossing: the beat has happened. The interval since the last one is
         * the rhythm. */
        rh_millis now = 0u;
        if (pulse->timer != NULL && pulse->timer->get_millis != NULL) {
            now = pulse->timer->get_millis(pulse->timer);
        }
        if (pulse->last_beat_ms != 0u) {
            rh_millis interval = now - pulse->last_beat_ms;
            if (interval > 0u) {
                pulse->beats_per_minute = (rh_u16)(60000u / (rh_u32)interval);
            }
        }
        pulse->last_beat_ms = now;
        pulse->beat = RH_TRUE;
    } else {
        pulse->beat = RH_FALSE;
    }
    pulse->above = over;

    *beats_per_minute = pulse->beats_per_minute;
    if (beat != NULL) {
        *beat = pulse->beat;
    }
    return ROBOHUB_OK;
}

RoboHubError robohub_pulse_adc_init(RoboHubPulseAdc* pulse,
                                    RoboHubADC* adc,
                                    RoboHubTimer* timer,
                                    rh_u8 channel,
                                    rh_f32 threshold)
{
    if (pulse == NULL || adc == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    pulse->adc = adc;
    pulse->timer = timer;
    pulse->channel = channel;
    pulse->threshold = threshold;
    pulse->above = RH_FALSE;
    pulse->last_beat_ms = 0u;
    pulse->beats_per_minute = 0u;
    pulse->beat = RH_FALSE;
    pulse->ready = RH_FALSE;

    pulse->interface.name = "pulse";
    pulse->interface.context = pulse;
    pulse->interface.init = pulse_op_init;
    pulse->interface.deinit = pulse_op_deinit;
    pulse->interface.read = pulse_op_read;

    return pulse_op_init(&pulse->interface);
}
