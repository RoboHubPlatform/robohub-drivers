/**
 * @file thermistor_adc.h
 * @brief Driver - a thermistor on an ADC, as a RoboHubTemperature.
 *
 * A thermistor's resistance falls as it warms, and a resistor beside it turns
 * that into a voltage a converter can read. The arithmetic from there to degrees
 * is the part's: the β coefficient and the resistance at a known temperature.
 *
 * The defaults are the ones the KY-013 and the usual 10 kΩ part use — β of 3950,
 * 10 kΩ at 25 °C, a 10 kΩ pull-up — and a part that differs is a different
 * number here and nowhere else.
 *
 * `invert` is the wiring: true when the thermistor is the *upper* half of the
 * divider, so that more light... more heat means a *lower* reading. The driver
 * reports degrees either way.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef THERMISTOR_ADC_H
#define THERMISTOR_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/adc.h"
#include "robohub/drivers/temperature.h"

/** The β coefficient of a 10 kΩ NTC. */
#define ROBOHUB_THERMISTOR_BETA 3950.0f
/** The resistance of a 10 kΩ NTC at 25 °C. */
#define ROBOHUB_THERMISTOR_NOMINAL_OHMS 10000.0f
/** The temperature that nominal resistance is quoted at. */
#define ROBOHUB_THERMISTOR_NOMINAL_C 25.0f
/** The divider's other resistor. */
#define ROBOHUB_THERMISTOR_PULLUP_OHMS 10000.0f

/** One thermistor on one ADC channel. */
typedef struct {
    RoboHubTemperature interface; /* Must remain the first member. */
    RoboHubADC*  adc;
    rh_u8        channel;
    rh_f32       beta;
    rh_f32       nominal_ohms;
    rh_f32       nominal_celsius;
    rh_f32       pullup_ohms;
    rh_bool      invert;   /* The thermistor is the upper half of the divider. */
    rh_bool      ready;
} RoboHubThermistorAdc;

/**
 * @brief Bind a thermistor to an ADC channel.
 *
 * The part's numbers are the defaults; a part that differs is a different
 * `beta`, `nominal_ohms`, `nominal_celsius` or `pullup_ohms`.
 */
RoboHubError robohub_thermistor_adc_init(RoboHubThermistorAdc* sensor,
                                        RoboHubADC* adc,
                                        rh_u8 channel,
                                        rh_bool invert);

#ifdef __cplusplus
}
#endif

#endif /* THERMISTOR_ADC_H */
