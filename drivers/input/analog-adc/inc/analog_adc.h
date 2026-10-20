/**
 * @file analog_adc.h
 * @brief Driver - an analog input on an ADC, as a RoboHubAnalog.
 *
 * A part that says a voltage: a hall sensor that follows a field, a potentiometer,
 * a strain gauge. The driver reads the converter and reports the level, and
 * `invert` says which way the part's output goes — a hall sensor sitting at half
 * scale with no field is the case that matters, and a caller should not have to
 * know whether its zero is up or down.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef ANALOG_ADC_H
#define ANALOG_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/adc.h"
#include "robohub/drivers/analog.h"

/** One analog input on one ADC channel. */
typedef struct {
    RoboHubAnalog interface; /* Must remain the first member. */
    RoboHubADC*   adc;
    rh_u8         channel;
    rh_bool       invert;  /* The part's output falls as its quantity rises. */
    rh_bool       ready;
} RoboHubAnalogAdc;

/** Bind an analog input to an ADC channel. */
RoboHubError robohub_analog_adc_init(RoboHubAnalogAdc* sensor,
                                     RoboHubADC* adc,
                                     rh_u8 channel,
                                     rh_bool invert);

#ifdef __cplusplus
}
#endif

#endif /* ANALOG_ADC_H */
