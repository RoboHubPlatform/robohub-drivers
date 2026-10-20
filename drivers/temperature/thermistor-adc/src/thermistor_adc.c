/*
 * thermistor_adc.c — a thermistor on an ADC.
 *
 * The divider gives a voltage, the voltage gives a resistance, and the resistance
 * gives a temperature through the part's own β coefficient. The last of those is
 * the only one that is arithmetic rather than wiring, which is why the part's
 * numbers are the driver's and not the caller's.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "thermistor_adc.h"

#include <math.h>
#include <stddef.h>

/* The resistance the divider implies for a reading. The two wirings differ in
 * which half of the divider the thermistor is. */
static rh_f32 thermistor_ohms(const RoboHubThermistorAdc* sensor, rh_f32 level)
{
    if (sensor->invert) {
        /* The thermistor is the upper half: the level falls as it warms. */
        if (level >= 1.0f) {
            return 0.0f;
        }
        return sensor->pullup_ohms * (1.0f - level) / level;
    }
    /* The thermistor is the lower half: the level rises as it warms. */
    if (level <= 0.0f) {
        return 0.0f;
    }
    return sensor->pullup_ohms * level / (1.0f - level);
}

static RoboHubError temp_op_init(RoboHubTemperature* self)
{
    RoboHubThermistorAdc* sensor = (RoboHubThermistorAdc*)self->context;
    if (sensor->adc == NULL || sensor->adc->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    sensor->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError temp_op_deinit(RoboHubTemperature* self)
{
    ((RoboHubThermistorAdc*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError temp_op_get_celsius(RoboHubTemperature* self, rh_f32* celsius)
{
    RoboHubThermistorAdc* sensor = (RoboHubThermistorAdc*)self->context;
    if (celsius == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!sensor->ready) {
        return ROBOHUB_ERR_STATE;
    }

    rh_f32 level = 0.0f;
    RoboHubError err = sensor->adc->read(sensor->adc, sensor->channel, &level);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_f32 ohms = thermistor_ohms(sensor, level);
    if (ohms <= 0.0f) {
        /* At the end of the divider the temperature is not a number a caller can
         * use; the reading is out of the part's range. */
        *celsius = sensor->invert ? -100.0f : 150.0f;
        return ROBOHUB_OK;
    }

    /* 1/T = 1/T0 + (1/β)·ln(R/R0), in kelvin. */
    rh_f32 t0 = sensor->nominal_celsius + 273.15f;
    rh_f32 ratio = ohms / sensor->nominal_ohms;
    rh_f32 inverse = (1.0f / t0) + (logf(ratio) / sensor->beta);
    if (inverse <= 0.0f) {
        *celsius = sensor->nominal_celsius;
        return ROBOHUB_OK;
    }
    *celsius = (1.0f / inverse) - 273.15f;
    return ROBOHUB_OK;
}

static RoboHubError temp_op_get_fahrenheit(RoboHubTemperature* self, rh_f32* fahrenheit)
{
    rh_f32 celsius = 0.0f;
    if (fahrenheit == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubError err = temp_op_get_celsius(self, &celsius);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *fahrenheit = celsius * 9.0f / 5.0f + 32.0f;
    return ROBOHUB_OK;
}

RoboHubError robohub_thermistor_adc_init(RoboHubThermistorAdc* sensor,
                                        RoboHubADC* adc,
                                        rh_u8 channel,
                                        rh_bool invert)
{
    if (sensor == NULL || adc == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    sensor->adc = adc;
    sensor->channel = channel;
    sensor->beta = ROBOHUB_THERMISTOR_BETA;
    sensor->nominal_ohms = ROBOHUB_THERMISTOR_NOMINAL_OHMS;
    sensor->nominal_celsius = ROBOHUB_THERMISTOR_NOMINAL_C;
    sensor->pullup_ohms = ROBOHUB_THERMISTOR_PULLUP_OHMS;
    sensor->invert = invert;
    sensor->ready = RH_FALSE;

    sensor->interface.name = "thermistor";
    sensor->interface.context = sensor;
    sensor->interface.init = temp_op_init;
    sensor->interface.deinit = temp_op_deinit;
    sensor->interface.get_celsius = temp_op_get_celsius;
    sensor->interface.get_fahrenheit = temp_op_get_fahrenheit;

    return temp_op_init(&sensor->interface);
}
