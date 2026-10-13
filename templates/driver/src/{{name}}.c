/**
 * @file {{name}}.c
 * @brief {{description}}
 *
 * The part's protocol, written against `RoboHubBus` — the common register bus —
 * and the driver interface it presents. Nothing here names I2C, SPI, a chip or a
 * vendor SDK: the bus is a contract a platform implements, so the same object
 * file runs wherever the bus does.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "{{name}}.h"

#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                      */
/* -------------------------------------------------------------------------- */

static RoboHubError {{name}}_op_init(RoboHubTemperature* self)
{
    RoboHub{{name}}Device* dev = (RoboHub{{name}}Device*)self->context;
    if (dev->bus == NULL || dev->bus->read_reg == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    /* Probe the part and read whatever it needs to come up, through
     * dev->bus->read_reg / dev->bus->write_reg, then: */
    dev->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError {{name}}_op_deinit(RoboHubTemperature* self)
{
    ((RoboHub{{name}}Device*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError {{name}}_op_get_celsius(RoboHubTemperature* self, rh_f32* celsius)
{
    RoboHub{{name}}Device* dev = (RoboHub{{name}}Device*)self->context;
    if (celsius == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!dev->ready) {
        return ROBOHUB_ERR_STATE;
    }
    /* Read the part and convert. Return ROBOHUB_ERR_PROTOCOL when a check the
     * part offers — a checksum, an identity register — does not agree. */
    (void)dev;
    *celsius = 0.0f;
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_{{name}}_init(RoboHub{{name}}Device* dev, const RoboHub{{name}}Config* config)
{
    if (dev == NULL || config == NULL || config->bus == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->bus = config->bus;
    dev->timer = config->timer;
    dev->ready = RH_FALSE;

    dev->interface.name = "{{name}}";
    dev->interface.context = dev;
    dev->interface.init = {{name}}_op_init;
    dev->interface.deinit = {{name}}_op_deinit;
    dev->interface.get_celsius = {{name}}_op_get_celsius;

    return {{name}}_op_init(&dev->interface);
}

RoboHubError robohub_{{name}}_deinit(RoboHub{{name}}Device* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dev->ready = RH_FALSE;
    return ROBOHUB_OK;
}
