/*
 * bsp.c — Support package for the DHT11 sensor module.
 *
 * The whole of it is the join: the registry holds the platform's DHT line and
 * timer, the driver holds the device, and this looks the two up by the names the
 * caller gives and hands them to the driver. It owns nothing and keeps nothing,
 * so there is no state here to get wrong.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "bsp.h"
#include "registry.h"

#include <stddef.h>

/* -------------------------------------------------------------------------- */
/*  Defaults                                                                  */
/* -------------------------------------------------------------------------- */

static const RoboHubDHT11ModuleConfig dht11_module_default = {
    .dht = RH_DHT11_MODULE_DHT_NAME,
    .timer = RH_DHT11_MODULE_TIMER_NAME,
    .min_interval_ms = RH_DHT11_MODULE_MIN_INTERVAL_MS,
};

const RoboHubDHT11ModuleConfig* robohub_dht11_module_default_config(void)
{
    return &dht11_module_default;
}

/* -------------------------------------------------------------------------- */
/*  Board init                                                                */
/* -------------------------------------------------------------------------- */

void robohub_dht11_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up: the part is powered from the
     * module's VCC, has no reset line, and the line it is on belongs to the
     * platform. The hook exists so that fitting a module reads the same as
     * bringing a board up. */
}

/* -------------------------------------------------------------------------- */
/*  Module init                                                               */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_dht11_module_init(RoboHubDHT11* dev,
                                       const RoboHubDHT11ModuleConfig* config)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config == NULL) {
        config = robohub_dht11_module_default_config();
    }

    rh_name dht_name = (config->dht != NULL && config->dht[0] != '\0')
        ? config->dht : RH_DHT11_MODULE_DHT_NAME;
    rh_name timer_name = (config->timer != NULL && config->timer[0] != '\0')
        ? config->timer : RH_DHT11_MODULE_TIMER_NAME;

    RoboHubDHT* bus = robohub_get_dht(dht_name);
    if (bus == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    RoboHubTimer* timer = robohub_get_timer(timer_name);
    if (timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }

    RoboHubDHT11Config driver_config = {
        .bus = bus,
        .timer = timer,
        .min_interval_ms = config->min_interval_ms,
    };
    return robohub_dht11_init(dev, &driver_config);
}

/* -------------------------------------------------------------------------- */
/*  Module convenience                                                        */
/* -------------------------------------------------------------------------- */

/* The one sensor the breakout carries, kept in the board so that a caller with
 * no way to own a C struct can still have a reading. */
static RoboHubDHT11 module_device;
static rh_bool module_open = RH_FALSE;

RoboHubError robohub_dht11_module_open(void)
{
    RoboHubError err = robohub_dht11_module_init(&module_device, NULL);
    if (ROBOHUB_SUCCEEDED(err)) {
        module_open = RH_TRUE;
    }
    return err;
}

RoboHubError robohub_dht11_module_close(void)
{
    if (!module_open) {
        return ROBOHUB_ERR_STATE;
    }
    module_open = RH_FALSE;
    return robohub_dht11_deinit(&module_device);
}

RoboHubError robohub_dht11_module_read(rh_f32* celsius, rh_f32* percent)
{
    if (celsius == NULL && percent == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!module_open) {
        return ROBOHUB_ERR_STATE;
    }
    return robohub_dht11_read(&module_device, celsius, percent);
}

rh_i32 robohub_dht11_module_milli_celsius(void)
{
    rh_f32 celsius = 0.0f;
    if (ROBOHUB_FAILED(robohub_dht11_module_read(&celsius, NULL))) {
        return RH_DHT11_MODULE_ERROR;
    }
    return (rh_i32)(celsius * 1000.0f + (celsius >= 0.0f ? 0.5f : -0.5f));
}

rh_i32 robohub_dht11_module_milli_percent(void)
{
    rh_f32 percent = 0.0f;
    if (ROBOHUB_FAILED(robohub_dht11_module_read(NULL, &percent))) {
        return RH_DHT11_MODULE_ERROR;
    }
    return (rh_i32)(percent * 1000.0f + 0.5f);
}

void robohub_dht11_module_sleep_ms(rh_u32 ms)
{
    RoboHubTimer* timer = robohub_get_timer(RH_DHT11_MODULE_TIMER_NAME);
    if (timer != NULL && timer->delay_ms != NULL) {
        timer->delay_ms(timer, ms);
    }
}
