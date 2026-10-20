/*
 * analog_adc.c — an analog input on an ADC.
 *
 * The converter gives a level; the wiring says which way the part's quantity
 * goes, and the driver reports the quantity's direction either way.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "analog_adc.h"

#include <stddef.h>

static RoboHubError analog_op_init(RoboHubAnalog* self)
{
    RoboHubAnalogAdc* sensor = (RoboHubAnalogAdc*)self->context;
    if (sensor->adc == NULL || sensor->adc->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    sensor->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError analog_op_deinit(RoboHubAnalog* self)
{
    ((RoboHubAnalogAdc*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError analog_op_get_level(RoboHubAnalog* self, rh_f32* level)
{
    RoboHubAnalogAdc* sensor = (RoboHubAnalogAdc*)self->context;
    if (level == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!sensor->ready) {
        return ROBOHUB_ERR_STATE;
    }
    rh_f32 read = 0.0f;
    RoboHubError err = sensor->adc->read(sensor->adc, sensor->channel, &read);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *level = sensor->invert ? (1.0f - read) : read;
    return ROBOHUB_OK;
}

RoboHubError robohub_analog_adc_init(RoboHubAnalogAdc* sensor,
                                     RoboHubADC* adc,
                                     rh_u8 channel,
                                     rh_bool invert)
{
    if (sensor == NULL || adc == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    sensor->adc = adc;
    sensor->channel = channel;
    sensor->invert = invert;
    sensor->ready = RH_FALSE;

    sensor->interface.name = "analog";
    sensor->interface.context = sensor;
    sensor->interface.init = analog_op_init;
    sensor->interface.deinit = analog_op_deinit;
    sensor->interface.get_level = analog_op_get_level;

    return analog_op_init(&sensor->interface);
}
