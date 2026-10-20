/**
 * @file photoresistor_adc.h
 * @brief Driver - a photoresistor on an ADC, as a RoboHubLight.
 *
 * A photoresistor is one half of a divider: its resistance falls as light rises,
 * so the voltage across it rises or falls depending on which half it is. The
 * driver reads the converter and reports the light; `invert` says which way the
 * divider is wired, so a caller never has to.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef PHOTORESISTOR_ADC_H
#define PHOTORESISTOR_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/adc.h"
#include "robohub/drivers/light.h"

typedef struct {
    RoboHubLight interface; /* Must remain the first member. */
    RoboHubADC*  adc;
    rh_u8        channel;
    rh_bool      invert; /* True when more light reads a lower level. */
    rh_bool      ready;
} RoboHubPhotoresistorAdc;

/**
 * @brief Bind a photoresistor to an ADC channel.
 *
 * `invert` is the wiring: true when the part is the upper half of the divider, so
 * its voltage falls as light rises.
 */
RoboHubError robohub_photoresistor_adc_init(RoboHubPhotoresistorAdc* light,
                                            RoboHubADC* adc,
                                            rh_u8 channel,
                                            rh_bool invert);

#ifdef __cplusplus
}
#endif

#endif /* PHOTORESISTOR_ADC_H */
