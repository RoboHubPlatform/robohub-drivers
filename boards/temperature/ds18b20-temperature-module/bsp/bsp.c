/*
 * bsp.c — Support package for the DS18B20 temperature sensor module.
 *
 * This is the registry-specific layer of the driver stack: the bus comes from
 * the platform's registry, the driver holds only the device's protocol, and this
 * joins them. It looks the platform's 1-Wire bus and timer up by the names the
 * caller gives and hands them to the driver. It owns nothing and keeps nothing,
 * so there is no state here to get wrong.
 *
 * A 1-Wire part is reached through `RoboHubOneWire` directly rather than through
 * the common register bus, because 1-Wire is not register-shaped. The layering is
 * the same either way.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ds18b20_temperature_module.h"
#include "registry.h"

#include <stddef.h>

/* -------------------------------------------------------------------------- */
/*  Defaults                                                                  */
/* -------------------------------------------------------------------------- */

static const RoboHubDS18B20ModuleConfig ds18b20_module_default = {
    .onewire = RH_DS18B20_MODULE_ONEWIRE_NAME,
    .timer = RH_DS18B20_MODULE_TIMER_NAME,
    .resolution_bits = RH_DS18B20_MODULE_RESOLUTION,
    .alarm_high_c = 30,
    .alarm_low_c = 0,
};

const RoboHubDS18B20ModuleConfig* robohub_ds18b20_module_default_config(void)
{
    return &ds18b20_module_default;
}

/* -------------------------------------------------------------------------- */
/*  Board init                                                                */
/* -------------------------------------------------------------------------- */

void robohub_ds18b20_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up: the part is powered from the
     * module's VCC, has no reset line, and the bus it is on belongs to the
     * platform. The hook exists so that fitting a module reads the same as
     * bringing a board up, and so a carrier that does need something has a
     * place to put it. */
}

/* -------------------------------------------------------------------------- */
/*  Module init                                                               */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_ds18b20_module_init(RoboHubDS18B20* dev,
                                         const RoboHubDS18B20ModuleConfig* config)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config == NULL) {
        config = robohub_ds18b20_module_default_config();
    }

    rh_name bus_name = (config->onewire != NULL && config->onewire[0] != '\0')
        ? config->onewire
        : RH_DS18B20_MODULE_ONEWIRE_NAME;
    rh_name timer_name = (config->timer != NULL && config->timer[0] != '\0')
        ? config->timer
        : RH_DS18B20_MODULE_TIMER_NAME;

    RoboHubOneWire* bus = robohub_get_onewire(bus_name);
    if (bus == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    RoboHubTimer* timer = robohub_get_timer(timer_name);
    if (timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }

    RoboHubDS18B20Config driver_config = {
        .bus = bus,
        .timer = timer,
        .resolution_bits = config->resolution_bits,
        .alarm_high_c = config->alarm_high_c,
        .alarm_low_c = config->alarm_low_c,
    };
    return robohub_ds18b20_init(dev, &driver_config);
}

/* -------------------------------------------------------------------------- */
/*  Module convenience                                                        */
/* -------------------------------------------------------------------------- */

/* The one sensor the breakout carries, kept in the board so that a caller with
 * no way to own a C struct can still have a reading. */
static RoboHubDS18B20 module_device;
static rh_bool module_open = RH_FALSE;

RoboHubError robohub_ds18b20_module_open(void)
{
    RoboHubError err = robohub_ds18b20_module_init(&module_device, NULL);
    if (ROBOHUB_SUCCEEDED(err)) {
        module_open = RH_TRUE;
    }
    return err;
}

RoboHubError robohub_ds18b20_module_close(void)
{
    if (!module_open) {
        return ROBOHUB_ERR_STATE;
    }
    module_open = RH_FALSE;
    return robohub_ds18b20_deinit(&module_device);
}

RoboHubError robohub_ds18b20_module_read(rh_f32* celsius)
{
    if (celsius == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!module_open) {
        return ROBOHUB_ERR_STATE;
    }
    return robohub_ds18b20_get_celsius(&module_device, celsius);
}

rh_f32 robohub_ds18b20_module_celsius(void)
{
    rh_f32 celsius = 0.0f;
    if (ROBOHUB_FAILED(robohub_ds18b20_module_read(&celsius))) {
        return -1000.0f;
    }
    return celsius;
}

rh_i32 robohub_ds18b20_module_milli_celsius(void)
{
    rh_f32 celsius = 0.0f;
    if (ROBOHUB_FAILED(robohub_ds18b20_module_read(&celsius))) {
        return RH_DS18B20_MODULE_TEMP_ERROR;
    }
    return (rh_i32)(celsius * 1000.0f + (celsius >= 0.0f ? 0.5f : -0.5f));
}

void robohub_ds18b20_module_sleep_ms(rh_u32 ms)
{
    RoboHubTimer* timer = robohub_get_timer(RH_DS18B20_MODULE_TIMER_NAME);
    if (timer != NULL && timer->delay_ms != NULL) {
        timer->delay_ms(timer, ms);
    }
}
