/*
 * bmp180.c — the Bosch BMP180 temperature, pressure and altitude sensor.
 *
 * Everything here is the part's protocol and its arithmetic. It reaches the
 * device through a `RoboHubBus` — the common register bus, which is I2C or SPI
 * without this file knowing which — and the conversion wait through a
 * `RoboHubTimer` when the caller bound one. It names no chip and no vendor SDK,
 * which is what lets it be compiled for any board.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "bmp180.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Protocol constants                                                        */
/* -------------------------------------------------------------------------- */

/* Registers, as the datasheet numbers them. */
#define BMP180_REG_CHIP_ID   0xD0u
#define BMP180_REG_CALIB     0xAAu
#define BMP180_REG_CTRL      0xF4u
#define BMP180_REG_DATA      0xF6u

#define BMP180_CALIB_BYTES   22u
#define BMP180_DATA_BYTES    3u

/* The command written to the control register to start a conversion. */
#define BMP180_CMD_TEMP      0x2Eu
#define BMP180_CMD_PRESSURE  0x34u
#define BMP180_PRESSURE_OSS_SHIFT 6u

/* End-of-conversion: bit 5 of the control register reads as one when the part
 * has finished. Used only when the caller bound no timer. */
#define BMP180_CTRL_EOC      0x20u

/* The conversion time, in milliseconds, the part needs by oversampling. Indexed
 * by the oversampling setting. Rounded up from the datasheet. */
static const rh_u16 bmp180_pressure_ms[4] = { 5u, 8u, 14u, 26u };

/* The temperature conversion is a fixed 4.5 ms. */
#define BMP180_TEMP_MS 5u

/* How many times the control register is polled for the end-of-conversion bit
 * when no timer was bound. */
#define BMP180_POLL_LIMIT 20000u

/* -------------------------------------------------------------------------- */
/*  Helpers                                                                   */
/* -------------------------------------------------------------------------- */

static RoboHubError bmp180_read(RoboHubBMP180* dev, rh_u8 reg, rh_u8* data, rh_size len)
{
    if (dev->bus == NULL || dev->bus->read_reg == NULL) {
        return ROBOHUB_ERR_STATE;
    }
    return dev->bus->read_reg(dev->bus, reg, data, len);
}

static RoboHubError bmp180_write(RoboHubBMP180* dev, rh_u8 reg, rh_u8 value)
{
    if (dev->bus == NULL || dev->bus->write_reg == NULL) {
        return ROBOHUB_ERR_STATE;
    }
    return dev->bus->write_reg(dev->bus, reg, &value, 1u);
}

/* Wait out a conversion. With a timer, sleep for the part's own conversion time.
 * Without one, poll the end-of-conversion bit rather than guessing, which is
 * exact regardless of how fast the bus is. */
static RoboHubError bmp180_wait(RoboHubBMP180* dev, rh_u32 ms)
{
    if (dev->timer != NULL && dev->timer->delay_ms != NULL) {
        dev->timer->delay_ms(dev->timer, ms);
        return ROBOHUB_OK;
    }

    for (rh_u32 attempt = 0; attempt < BMP180_POLL_LIMIT; attempt++) {
        rh_u8 ctrl = 0u;
        RoboHubError err = bmp180_read(dev, BMP180_REG_CTRL, &ctrl, 1u);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
        if ((ctrl & BMP180_CTRL_EOC) != 0u) {
            return ROBOHUB_OK;
        }
    }
    return ROBOHUB_ERR_TIMEOUT;
}

static rh_i16 bmp180_i16(const rh_u8* p)
{
    return (rh_i16)(((rh_u16)p[0] << 8) | (rh_u16)p[1]);
}

/* Start a conversion and wait for it: write the command, then wait the part's
 * conversion time. Both readings start this way, which is why it is one
 * function and not two. */
static RoboHubError bmp180_convert(RoboHubBMP180* dev, rh_u8 command, rh_u32 wait_ms)
{
    RoboHubError err = bmp180_write(dev, BMP180_REG_CTRL, command);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    return bmp180_wait(dev, wait_ms);
}

/* The uncompensated temperature, a 16-bit value at the data registers. */
static RoboHubError bmp180_read_ut(RoboHubBMP180* dev, rh_i32* ut)
{
    RoboHubError err = bmp180_convert(dev, BMP180_CMD_TEMP, BMP180_TEMP_MS);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_u8 data[BMP180_DATA_BYTES] = { 0u, 0u, 0u };
    err = bmp180_read(dev, BMP180_REG_DATA, data, 2u);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *ut = (rh_i32)(((rh_u16)data[0] << 8) | (rh_u16)data[1]);
    return ROBOHUB_OK;
}

/* The uncompensated pressure. The part reports more bits the more it oversamples,
 * and the datasheet's shift is how they are brought back to a common scale. */
static RoboHubError bmp180_read_up(RoboHubBMP180* dev, rh_i32* up)
{
    rh_u8 command = (rh_u8)((dev->oversampling << BMP180_PRESSURE_OSS_SHIFT) | BMP180_CMD_PRESSURE);
    RoboHubError err = bmp180_convert(dev, command, bmp180_pressure_ms[dev->oversampling]);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_u8 data[BMP180_DATA_BYTES] = { 0u, 0u, 0u };
    err = bmp180_read(dev, BMP180_REG_DATA, data, BMP180_DATA_BYTES);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    rh_i32 raw = (rh_i32)(((rh_u32)data[0] << 16) | ((rh_u32)data[1] << 8) | (rh_u32)data[2]);
    *up = raw >> (8u - dev->oversampling);
    return ROBOHUB_OK;
}

/* Bring the part up: read the factory calibration and confirm the identity.
 * Called once from init, and again by an interface's own init. */
static RoboHubError bmp180_bring_up(RoboHubBMP180* dev)
{
    rh_u8 chip_id = 0u;
    RoboHubError err = bmp180_read(dev, BMP180_REG_CHIP_ID, &chip_id, 1u);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    if (chip_id != ROBOHUB_BMP180_CHIP_ID) {
        return ROBOHUB_ERR_PROTOCOL;
    }

    rh_u8 calib[BMP180_CALIB_BYTES];
    err = bmp180_read(dev, BMP180_REG_CALIB, calib, BMP180_CALIB_BYTES);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    dev->ac1 = bmp180_i16(&calib[0]);
    dev->ac2 = bmp180_i16(&calib[2]);
    dev->ac3 = bmp180_i16(&calib[4]);
    dev->ac4 = bmp180_i16(&calib[6]);
    dev->ac5 = bmp180_i16(&calib[8]);
    dev->ac6 = bmp180_i16(&calib[10]);
    dev->b1  = bmp180_i16(&calib[12]);
    dev->b2  = bmp180_i16(&calib[14]);
    dev->mb  = bmp180_i16(&calib[16]);
    dev->mc  = bmp180_i16(&calib[18]);
    dev->md  = bmp180_i16(&calib[20]);

    dev->ready = RH_TRUE;
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Compensation                                                              */
/* -------------------------------------------------------------------------- */

/* The shared intermediate B5, which the temperature is and the pressure needs.
 * The part's calibration is what makes this arithmetic its arithmetic. */
static RoboHubError bmp180_b5(RoboHubBMP180* dev, rh_i32 ut, rh_i32* b5)
{
    rh_i32 denominator = (((ut - dev->ac6) * dev->ac5) >> 15) + dev->md;
    if (denominator == 0) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    rh_i32 x1 = ((ut - dev->ac6) * dev->ac5) >> 15;
    rh_i32 x2 = (dev->mc << 11) / denominator;
    *b5 = x1 + x2;
    return ROBOHUB_OK;
}

/* The pressure in pascals, and the B5 it needed, so a caller reading both does
 * not compute it twice. */
static RoboHubError bmp180_pressure_pa(RoboHubBMP180* dev, rh_i32 ut, rh_i32 up, rh_i32* pa)
{
    rh_i32 b5 = 0;
    RoboHubError err = bmp180_b5(dev, ut, &b5);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_i32 b6 = b5 - 4000;
    rh_i32 x1 = (dev->b2 * ((b6 * b6) >> 12)) >> 11;
    rh_i32 x2 = (dev->ac2 * b6) >> 11;
    rh_i32 x3 = x1 + x2;
    rh_i32 b3 = (((dev->ac1 * 4 + x3) << dev->oversampling) + 2) >> 2;

    x1 = (dev->ac3 * b6) >> 13;
    x2 = (dev->b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;

    rh_u32 b4 = (rh_u32)((((rh_i64)dev->ac4) * (rh_i64)(x3 + 32768)) >> 15);
    if (b4 == 0u) {
        return ROBOHUB_ERR_PROTOCOL;
    }

    rh_i64 b7 = ((rh_i64)up - (rh_i64)b3) * (rh_i64)(50000u >> dev->oversampling);
    rh_i32 p;
    if (b7 < 0x80000000LL) {
        p = (rh_i32)((b7 * 2) / (rh_i64)b4);
    } else {
        p = (rh_i32)((b7 / (rh_i64)b4) * 2);
    }

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);

    *pa = p;
    return ROBOHUB_OK;
}

/* The temperature from the B5 the compensation already computed. */
static rh_f32 bmp180_temperature_celsius(rh_i32 b5)
{
    return (rh_f32)((b5 + 8) >> 4) / 10.0f;
}

/* The international barometric formula, against the configured reference. */
static rh_f32 bmp180_altitude_metres(rh_f32 pa, rh_f32 sea_level_pa)
{
    return 44330.0f * (1.0f - powf(pa / sea_level_pa, 1.0f / 5.255f));
}

/* -------------------------------------------------------------------------- */
/*  Temperature interface                                                     */
/* -------------------------------------------------------------------------- */

static RoboHubError bmp180_temp_init(RoboHubTemperature* self)
{
    return bmp180_bring_up((RoboHubBMP180*)self->context);
}

static RoboHubError bmp180_temp_deinit(RoboHubTemperature* self)
{
    ((RoboHubBMP180*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError bmp180_temp_celsius(RoboHubTemperature* self, rh_f32* celsius)
{
    if (celsius == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return robohub_bmp180_read((RoboHubBMP180*)self->context, celsius, NULL, NULL);
}

static RoboHubError bmp180_temp_fahrenheit(RoboHubTemperature* self, rh_f32* fahrenheit)
{
    if (fahrenheit == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rh_f32 celsius = 0.0f;
    RoboHubError err = bmp180_temp_celsius(self, &celsius);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *fahrenheit = celsius * 9.0f / 5.0f + 32.0f;
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Pressure interface                                                        */
/* -------------------------------------------------------------------------- */

static RoboHubError bmp180_press_init(RoboHubPressure* self)
{
    return bmp180_bring_up((RoboHubBMP180*)self->context);
}

static RoboHubError bmp180_press_deinit(RoboHubPressure* self)
{
    ((RoboHubBMP180*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError bmp180_press_get(RoboHubPressure* self, rh_f32* pascals)
{
    if (pascals == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return robohub_bmp180_read((RoboHubBMP180*)self->context, NULL, pascals, NULL);
}

/* -------------------------------------------------------------------------- */
/*  Altitude interface                                                        */
/* -------------------------------------------------------------------------- */

static RoboHubError bmp180_alt_init(RoboHubAltitude* self)
{
    return bmp180_bring_up((RoboHubBMP180*)self->context);
}

static RoboHubError bmp180_alt_deinit(RoboHubAltitude* self)
{
    ((RoboHubBMP180*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError bmp180_alt_set_reference(RoboHubAltitude* self, rh_f32 sea_level_pa)
{
    RoboHubBMP180* dev = (RoboHubBMP180*)self->context;
    if (sea_level_pa <= 0.0f) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dev->sea_level_pa = sea_level_pa;
    return ROBOHUB_OK;
}

static RoboHubError bmp180_alt_get(RoboHubAltitude* self, rh_f32* metres)
{
    if (metres == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return robohub_bmp180_read((RoboHubBMP180*)self->context, NULL, NULL, metres);
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_bmp180_init(RoboHubBMP180* dev, const RoboHubBMP180Config* config)
{
    if (dev == NULL || config == NULL || config->bus == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config->oversampling > ROBOHUB_BMP180_OSS_MAX) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->bus = config->bus;
    dev->timer = config->timer;
    dev->oversampling = config->oversampling;
    dev->sea_level_pa = config->sea_level_pa > 0.0f
        ? config->sea_level_pa
        : ROBOHUB_SEA_LEVEL_PRESSURE_PA;
    dev->ready = RH_FALSE;

    dev->temperature.name = "bmp180.temperature";
    dev->temperature.context = dev;
    dev->temperature.init = bmp180_temp_init;
    dev->temperature.deinit = bmp180_temp_deinit;
    dev->temperature.get_celsius = bmp180_temp_celsius;
    dev->temperature.get_fahrenheit = bmp180_temp_fahrenheit;

    dev->pressure.name = "bmp180.pressure";
    dev->pressure.context = dev;
    dev->pressure.init = bmp180_press_init;
    dev->pressure.deinit = bmp180_press_deinit;
    dev->pressure.get_pressure = bmp180_press_get;

    dev->altitude.name = "bmp180.altitude";
    dev->altitude.context = dev;
    dev->altitude.init = bmp180_alt_init;
    dev->altitude.deinit = bmp180_alt_deinit;
    dev->altitude.set_reference = bmp180_alt_set_reference;
    dev->altitude.get_altitude = bmp180_alt_get;

    return bmp180_bring_up(dev);
}

RoboHubError robohub_bmp180_deinit(RoboHubBMP180* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dev->ready = RH_FALSE;
    return ROBOHUB_OK;
}

RoboHubError robohub_bmp180_get_celsius(RoboHubBMP180* dev, rh_f32* celsius)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return bmp180_temp_celsius(&dev->temperature, celsius);
}

RoboHubError robohub_bmp180_get_pressure(RoboHubBMP180* dev, rh_f32* pascals)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return bmp180_press_get(&dev->pressure, pascals);
}

RoboHubError robohub_bmp180_get_altitude(RoboHubBMP180* dev, rh_f32* metres)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return bmp180_alt_get(&dev->altitude, metres);
}

RoboHubError robohub_bmp180_set_sea_level(RoboHubBMP180* dev, rh_f32 sea_level_pa)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return bmp180_alt_set_reference(&dev->altitude, sea_level_pa);
}

RoboHubError robohub_bmp180_read(RoboHubBMP180* dev,
                                 rh_f32* celsius,
                                 rh_f32* pascals,
                                 rh_f32* metres)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (celsius == NULL && pascals == NULL && metres == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!dev->ready) {
        return ROBOHUB_ERR_STATE;
    }

    /* The temperature conversion is always issued: its result is a value a
     * caller may want, and it is also the B5 the pressure arithmetic needs. */
    rh_i32 ut = 0;
    RoboHubError err = bmp180_read_ut(dev, &ut);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    if (pascals == NULL && metres == NULL) {
        rh_i32 b5 = 0;
        err = bmp180_b5(dev, ut, &b5);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
        *celsius = bmp180_temperature_celsius(b5);
        return ROBOHUB_OK;
    }

    rh_i32 up = 0;
    err = bmp180_read_up(dev, &up);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_i32 pa = 0;
    err = bmp180_pressure_pa(dev, ut, up, &pa);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    if (celsius != NULL) {
        rh_i32 b5 = 0;
        err = bmp180_b5(dev, ut, &b5);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
        *celsius = bmp180_temperature_celsius(b5);
    }
    if (pascals != NULL) {
        *pascals = (rh_f32)pa;
    }
    if (metres != NULL) {
        *metres = bmp180_altitude_metres((rh_f32)pa, dev->sea_level_pa);
    }
    return ROBOHUB_OK;
}
