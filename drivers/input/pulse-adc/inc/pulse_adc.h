/**
 * @file pulse_adc.h
 * @brief Driver - a pulse in an analog signal, as a RoboHubPulse.
 *
 * Some things are not a level but a rhythm: a heartbeat, a flicker, a flame. The
 * part reports a waveform and what a program wants is the rate at which
 * something happened, so the driver finds the peaks and counts the rhythm.
 *
 * The threshold is what counts as a beat — the level the signal has to cross —
 * and the interval between crossings is what gives the rate. A part that needs a
 * finer judgement than one threshold wants the samples, and this is the
 * judgement one threshold makes.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef PULSE_ADC_H
#define PULSE_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/adc.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/pulse.h"

/** One pulse source on one ADC channel. */
typedef struct {
    RoboHubPulse interface; /* Must remain the first member. */
    RoboHubADC*   adc;
    RoboHubTimer* timer;    /* For the interval between beats. */
    rh_u8         channel;
    rh_f32        threshold;    /* The level a beat has to cross. */
    rh_bool       above;        /* Whether the last sample was over it. */
    rh_millis     last_beat_ms;
    rh_u16        beats_per_minute;
    rh_bool       beat;         /* A beat passed since the last read. */
    rh_bool       ready;
} RoboHubPulseAdc;

/**
 * @brief Bind a pulse source to an ADC channel.
 *
 * `threshold` is the level a beat has to cross; a signal that rests below it and
 * rises over it beats.
 */
RoboHubError robohub_pulse_adc_init(RoboHubPulseAdc* pulse,
                                    RoboHubADC* adc,
                                    RoboHubTimer* timer,
                                    rh_u8 channel,
                                    rh_f32 threshold);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_ADC_H */
