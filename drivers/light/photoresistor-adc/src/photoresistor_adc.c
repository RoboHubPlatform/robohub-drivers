/*
 * photoresistor_adc.c — a photoresistor on an ADC.
 *
 * The converter gives a level; the wiring says whether more light is more or
 * less of it, and the driver reports light either way.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "photoresistor_adc.h"

#include <stddef.h>

static RoboHubError light_op_init(RoboHubLight* self)
{
    RoboHubPhotoresistorAdc* light = (RoboHubPhotoresistorAdc*)self->context;
    if (light->adc == NULL || light->adc->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    light->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError light_op_deinit(RoboHubLight* self)
{
    ((RoboHubPhotoresistorAdc*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError light_op_get(RoboHubLight* self, rh_f32* level)
{
    RoboHubPhotoresistorAdc* light = (RoboHubPhotoresistorAdc*)self->context;
    if (level == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!light->ready) {
        return ROBOHUB_ERR_STATE;
    }
    rh_f32 read = 0.0f;
    RoboHubError err = light->adc->read(light->adc, light->channel, &read);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *level = light->invert ? (1.0f - read) : read;
    return ROBOHUB_OK;
}

RoboHubError robohub_photoresistor_adc_init(RoboHubPhotoresistorAdc* light,
                                            RoboHubADC* adc,
                                            rh_u8 channel,
                                            rh_bool invert)
{
    if (light == NULL || adc == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    light->adc = adc;
    light->channel = channel;
    light->invert = invert;
    light->ready = RH_FALSE;

    light->interface.name = "photoresistor";
    light->interface.context = light;
    light->interface.init = light_op_init;
    light->interface.deinit = light_op_deinit;
    light->interface.get_level = light_op_get;

    return light_op_init(&light->interface);
}
