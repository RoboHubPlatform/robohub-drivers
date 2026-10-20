/*
 * bsp.c — Support package for the BMP180 sensor module.
 *
 * The whole of it is the join: the registry holds the platform's controller and
 * timer, the driver holds the device, and this looks the first two up by the
 * names the caller gives, binds the controller as the common register bus, and
 * hands the bus to the driver. It owns nothing and keeps nothing beyond the one
 * bound bus a breakout's single sensor needs, so there is no state here to get
 * wrong.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "bmp180_module.h"
#include "registry.h"

#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Defaults                                                                  */
/* -------------------------------------------------------------------------- */

static const RoboHubBMP180ModuleConfig bmp180_module_default = {
    .transport = RH_BMP180_MODULE_TRANSPORT_I2C,
    .i2c = RH_BMP180_MODULE_I2C_NAME,
    .address = RH_BMP180_MODULE_I2C_ADDRESS,
    .spi = RH_BMP180_MODULE_SPI_NAME,
    .cs = NULL,
    .cs_pin = 0,
    .read_flag = 0,
    .timer = RH_BMP180_MODULE_TIMER_NAME,
    .oversampling = RH_BMP180_MODULE_OSS,
    .sea_level_pa = RH_BMP180_MODULE_SEA_LEVEL_PA,
    .speed_hz = RH_BMP180_MODULE_SPEED_HZ,
};

const RoboHubBMP180ModuleConfig* robohub_bmp180_module_default_config(void)
{
    return &bmp180_module_default;
}

/* The one bus the breakout's single sensor is on, kept so the convenience calls
 * can reach the device without the caller owning a bus value. */
static union {
    RoboHubBusI2C i2c;
    RoboHubBusSPI spi;
} module_bus;

/* -------------------------------------------------------------------------- */
/*  Small helpers                                                             */
/* -------------------------------------------------------------------------- */

static rh_name pick(rh_name value, rh_name fallback)
{
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

/* -------------------------------------------------------------------------- */
/*  Board init                                                                */
/* -------------------------------------------------------------------------- */

void robohub_bmp180_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up: the part is powered from the
     * module's VCC, has no reset line, and the bus it is on belongs to the
     * platform. The hook exists so that fitting a module reads the same as
     * bringing a board up. */
}

/* -------------------------------------------------------------------------- */
/*  Module init                                                               */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_bmp180_module_init(RoboHubBMP180* dev,
                                        const RoboHubBMP180ModuleConfig* config)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config == NULL) {
        config = robohub_bmp180_module_default_config();
    }

    RoboHubTimer* timer = robohub_get_timer(pick(config->timer, RH_BMP180_MODULE_TIMER_NAME));
    if (timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }

    rh_u32 speed_hz = config->speed_hz != 0u ? config->speed_hz : RH_BMP180_MODULE_SPEED_HZ;
    RoboHubBus* bus = NULL;

    if (config->transport == RH_BMP180_MODULE_TRANSPORT_SPI) {
        RoboHubGPIO* cs = NULL;
        if (config->cs != NULL && config->cs[0] != '\0') {
            cs = robohub_get_gpio(config->cs);
            if (cs == NULL) {
                return ROBOHUB_ERR_NOT_FOUND;
            }
        }
        RoboHubError err = robohub_bus_spi_open(&module_bus.spi,
                                                pick(config->spi, RH_BMP180_MODULE_SPI_NAME),
                                                cs, config->cs_pin, config->read_flag, speed_hz);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
        bus = &module_bus.spi.bus;
    } else {
        rh_u8 address = config->address != 0u ? config->address : RH_BMP180_MODULE_I2C_ADDRESS;
        RoboHubError err = robohub_bus_i2c_open(&module_bus.i2c,
                                               pick(config->i2c, RH_BMP180_MODULE_I2C_NAME),
                                               address, speed_hz);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
        bus = &module_bus.i2c.bus;
    }

    RoboHubBMP180Config driver_config = {
        .bus = bus,
        .timer = timer,
        .oversampling = config->oversampling,
        .sea_level_pa = config->sea_level_pa,
    };
    return robohub_bmp180_init(dev, &driver_config);
}

/* -------------------------------------------------------------------------- */
/*  Module convenience                                                        */
/* -------------------------------------------------------------------------- */

/* The one sensor the breakout carries, kept in the board so that a caller with
 * no way to own a C struct can still have a reading. */
static RoboHubBMP180 module_device;
static rh_bool module_open = RH_FALSE;

RoboHubError robohub_bmp180_module_open(void)
{
    RoboHubError err = robohub_bmp180_module_init(&module_device, NULL);
    if (ROBOHUB_SUCCEEDED(err)) {
        module_open = RH_TRUE;
    }
    return err;
}

RoboHubError robohub_bmp180_module_close(void)
{
    if (!module_open) {
        return ROBOHUB_ERR_STATE;
    }
    module_open = RH_FALSE;
    return robohub_bmp180_deinit(&module_device);
}

RoboHubError robohub_bmp180_module_read(rh_f32* celsius, rh_f32* pascals, rh_f32* metres)
{
    if (celsius == NULL && pascals == NULL && metres == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!module_open) {
        return ROBOHUB_ERR_STATE;
    }
    return robohub_bmp180_read(&module_device, celsius, pascals, metres);
}

rh_i32 robohub_bmp180_module_milli_celsius(void)
{
    rh_f32 celsius = 0.0f;
    if (ROBOHUB_FAILED(robohub_bmp180_module_read(&celsius, NULL, NULL))) {
        return RH_BMP180_MODULE_ERROR;
    }
    return (rh_i32)(celsius * 1000.0f + (celsius >= 0.0f ? 0.5f : -0.5f));
}

rh_i32 robohub_bmp180_module_pascals(void)
{
    rh_f32 pascals = 0.0f;
    if (ROBOHUB_FAILED(robohub_bmp180_module_read(NULL, &pascals, NULL))) {
        return RH_BMP180_MODULE_ERROR;
    }
    return (rh_i32)(pascals + 0.5f);
}

rh_i32 robohub_bmp180_module_centi_metres(void)
{
    rh_f32 metres = 0.0f;
    if (ROBOHUB_FAILED(robohub_bmp180_module_read(NULL, NULL, &metres))) {
        return RH_BMP180_MODULE_ERROR;
    }
    return (rh_i32)(metres * 100.0f + (metres >= 0.0f ? 0.5f : -0.5f));
}

void robohub_bmp180_module_sleep_ms(rh_u32 ms)
{
    RoboHubTimer* timer = robohub_get_timer(RH_BMP180_MODULE_TIMER_NAME);
    if (timer != NULL && timer->delay_ms != NULL) {
        timer->delay_ms(timer, ms);
    }
}
