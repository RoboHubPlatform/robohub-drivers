/*
 * bsp.c — Support package for the {{name}} eval board.
 *
 * The join: the registry holds the platform's controller, the driver holds the
 * part, and this looks the controller up by the name the caller gives, binds it
 * as the common register bus, and hands it to the driver. It owns nothing.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "{{name}}_module.h"
#include "registry.h"

#include <stddef.h>

static const RoboHub{{name}}ModuleConfig {{name}}_module_default = {
    .i2c = RH_{{name}}_MODULE_BUS_NAME,
    .address = RH_{{name}}_MODULE_ADDRESS,
    .speed_hz = RH_{{name}}_MODULE_SPEED_HZ,
};

const RoboHub{{name}}ModuleConfig* robohub_{{name}}_module_default_config(void)
{
    return &{{name}}_module_default;
}

void robohub_{{name}}_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up: the part is powered from the
     * module's VCC and has no reset line. */
}

RoboHubError robohub_{{name}}_module_init(RoboHub{{name}}Device* dev,
                                          const RoboHub{{name}}ModuleConfig* config)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config == NULL) {
        config = robohub_{{name}}_module_default_config();
    }

    /* The bus the module is on, kept here for the one part a carrier holds. */
    static RoboHubBusI2C bus;
    rh_name name = (config->i2c != NULL && config->i2c[0] != '\0')
        ? config->i2c : RH_{{name}}_MODULE_BUS_NAME;
    rh_u32 speed = config->speed_hz != 0u ? config->speed_hz : RH_{{name}}_MODULE_SPEED_HZ;

    RoboHubError err = robohub_bus_i2c_open(&bus, name, config->address, speed);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    RoboHub{{name}}Config driver_config = { .bus = &bus.bus };
    return robohub_{{name}}_init(dev, &driver_config);
}
