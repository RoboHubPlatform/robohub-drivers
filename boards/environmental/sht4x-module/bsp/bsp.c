/*
 * bsp.c — an SHT4x breakout on I2C (DRV-BRD-0005, robohub ADR-0026).
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "sht4x_module.h"
#include "registry.h"

#include <stddef.h>

static const RoboHubSHT4xModuleConfig sht4x_module_default = {
    .i2c = RH_SHT4X_MODULE_I2C_NAME,
    .timer = RH_SHT4X_MODULE_TIMER_NAME,
    .address = RH_SHT4X_MODULE_ADDRESS,
    .speed_hz = RH_SHT4X_MODULE_SPEED_HZ,
};

const RoboHubSHT4xModuleConfig* robohub_sht4x_module_default_config(void)
{
    return &sht4x_module_default;
}

void robohub_sht4x_module_bsp_init(void)
{
    /* Nothing on the carrier to bring up: the part is powered from VCC, has no
     * reset line, and the bus belongs to the platform. */
}

static rh_name pick(rh_name value, rh_name fallback)
{
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

// @req DRV-BRD-0005
RoboHubError robohub_sht4x_module_init(RoboHubSHT4x* dev, const RoboHubSHT4xModuleConfig* config)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config == NULL) {
        config = robohub_sht4x_module_default_config();
    }
    RoboHubI2C* bus = robohub_get_i2c(pick(config->i2c, RH_SHT4X_MODULE_I2C_NAME));
    if (bus == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    RoboHubTimer* timer = robohub_get_timer(pick(config->timer, RH_SHT4X_MODULE_TIMER_NAME));
    if (timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    RoboHubSHT4xConfig driver_config = {
        .bus = bus,
        .timer = timer,
        .address = config->address,
        .speed_hz = config->speed_hz,
    };
    return robohub_sht4x_init(dev, &driver_config);
}
