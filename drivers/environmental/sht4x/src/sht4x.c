/*
 * sht4x.c — Sensirion's SHT40, SHT41 and SHT45 (DRV-ENV-0007…0009, robohub
 * ADR-0026).
 *
 * Everything here is the commands, the CRC and the arithmetic of the SHT4x
 * datasheet. The bus is the `RoboHubI2C` a platform implements and the wait the
 * `RoboHubTimer` the caller bound; this file names no chip and no pin.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "sht4x.h"
#include "robohub/drivers/temperature_utils.h"

#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  CRC and conversion                                                        */
/* -------------------------------------------------------------------------- */

// @req DRV-ENV-0008
rh_u8 robohub_sht4x_crc8(const rh_u8* data, rh_size length)
{
    rh_u8 crc = 0xFFu;
    for (rh_size i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (rh_u8)((crc & 0x80u) != 0u ? (rh_u8)((crc << 1) ^ 0x31u) : (rh_u8)(crc << 1));
        }
    }
    return crc;
}

// @req DRV-ENV-0008
rh_f32 robohub_sht4x_celsius(rh_u16 raw)
{
    return -45.0f + 175.0f * (rh_f32)raw / 65535.0f;
}

// @req DRV-ENV-0008
rh_f32 robohub_sht4x_percent(rh_u16 raw)
{
    rh_f32 percent = -6.0f + 125.0f * (rh_f32)raw / 65535.0f;
    if (percent < 0.0f) {
        return 0.0f;
    }
    if (percent > 100.0f) {
        return 100.0f;
    }
    return percent;
}

/* -------------------------------------------------------------------------- */
/*  The bus                                                                   */
/* -------------------------------------------------------------------------- */

static void sht4x_wait(RoboHubSHT4x* dev, rh_u32 ms)
{
    if (dev->timer != NULL && dev->timer->delay_ms != NULL) {
        dev->timer->delay_ms(dev->timer, ms);
    }
}

/* One command, its wait, and its six-byte answer checked word by word. */
// @req DRV-ENV-0007, DRV-ENV-0008, DRV-ENV-0009
static RoboHubError sht4x_command(RoboHubSHT4x* dev, rh_u8 command, rh_u32 wait_ms, rh_u16* first, rh_u16* second)
{
    rh_u8 cmd = command;
    if (ROBOHUB_FAILED(dev->bus->write(dev->bus, dev->address, &cmd, 1u))) {
        dev->needs_reset = RH_TRUE;
        return ROBOHUB_ERR_IO;
    }
    sht4x_wait(dev, wait_ms);
    rh_u8 answer[6];
    if (ROBOHUB_FAILED(dev->bus->read(dev->bus, dev->address, answer, sizeof(answer)))) {
        dev->needs_reset = RH_TRUE;
        return ROBOHUB_ERR_IO;
    }
    if (robohub_sht4x_crc8(&answer[0], 2u) != answer[2] || robohub_sht4x_crc8(&answer[3], 2u) != answer[5]) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    *first = (rh_u16)((answer[0] << 8) | answer[1]);
    *second = (rh_u16)((answer[3] << 8) | answer[4]);
    return ROBOHUB_OK;
}

// @req DRV-ENV-0009
static RoboHubError sht4x_reset(RoboHubSHT4x* dev)
{
    rh_u8 cmd = ROBOHUB_SHT4X_CMD_SOFT_RESET;
    if (ROBOHUB_FAILED(dev->bus->write(dev->bus, dev->address, &cmd, 1u))) {
        return ROBOHUB_ERR_IO;
    }
    sht4x_wait(dev, ROBOHUB_SHT4X_RESET_MS);
    dev->needs_reset = RH_FALSE;
    return ROBOHUB_OK;
}

/* Reset the part and read its serial number: a part that is not there fails
 * here, rather than reading as zero later. */
// @req DRV-ENV-0009
static RoboHubError sht4x_bring_up(RoboHubSHT4x* dev)
{
    if (dev->bus == NULL || dev->bus->write == NULL || dev->bus->read == NULL || dev->timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (dev->bus->init != NULL) {
        RoboHubError err = dev->bus->init(dev->bus, dev->speed_hz);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
    }
    dev->ready = RH_FALSE;
    if (ROBOHUB_FAILED(sht4x_reset(dev))) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    rh_u16 high = 0u;
    rh_u16 low = 0u;
    RoboHubError err = sht4x_command(dev, ROBOHUB_SHT4X_CMD_SERIAL, 1u, &high, &low);
    if (err == ROBOHUB_ERR_IO) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    dev->serial = ((rh_u32)high << 16) | low;
    dev->ready = RH_TRUE;
    return ROBOHUB_OK;
}

// @req DRV-ENV-0007, DRV-ENV-0008, DRV-ENV-0009
static RoboHubError sht4x_measure(RoboHubSHT4x* dev, rh_f32* celsius, rh_f32* percent)
{
    if (!dev->ready) {
        return ROBOHUB_ERR_STATE;
    }
    if (dev->needs_reset && ROBOHUB_FAILED(sht4x_reset(dev))) {
        return ROBOHUB_ERR_IO;
    }
    rh_u16 raw_t = 0u;
    rh_u16 raw_rh = 0u;
    RoboHubError err = sht4x_command(dev, ROBOHUB_SHT4X_CMD_MEASURE_HIGH, ROBOHUB_SHT4X_MEASURE_MS, &raw_t, &raw_rh);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    if (celsius != NULL) {
        *celsius = robohub_sht4x_celsius(raw_t);
    }
    if (percent != NULL) {
        *percent = robohub_sht4x_percent(raw_rh);
    }
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Interfaces                                                                */
/* -------------------------------------------------------------------------- */

static RoboHubError sht4x_temp_init(RoboHubTemperature* self)
{
    return sht4x_bring_up((RoboHubSHT4x*)self->context);
}

static RoboHubError sht4x_temp_deinit(RoboHubTemperature* self)
{
    ((RoboHubSHT4x*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError sht4x_temp_celsius(RoboHubTemperature* self, rh_f32* celsius)
{
    if (celsius == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return sht4x_measure((RoboHubSHT4x*)self->context, celsius, NULL);
}

static RoboHubError sht4x_temp_fahrenheit(RoboHubTemperature* self, rh_f32* fahrenheit)
{
    if (fahrenheit == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rh_f32 celsius = 0.0f;
    RoboHubError err = sht4x_measure((RoboHubSHT4x*)self->context, &celsius, NULL);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *fahrenheit = robohub_celsius_to_fahrenheit(celsius);
    return ROBOHUB_OK;
}

static RoboHubError sht4x_hum_init(RoboHubHumidity* self)
{
    return sht4x_bring_up((RoboHubSHT4x*)self->context);
}

static RoboHubError sht4x_hum_deinit(RoboHubHumidity* self)
{
    ((RoboHubSHT4x*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError sht4x_hum_get(RoboHubHumidity* self, rh_f32* percent)
{
    if (percent == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return sht4x_measure((RoboHubSHT4x*)self->context, NULL, percent);
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

// @req DRV-ENV-0007
RoboHubError robohub_sht4x_init(RoboHubSHT4x* dev, const RoboHubSHT4xConfig* config)
{
    if (dev == NULL || config == NULL || config->bus == NULL || config->timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    memset(dev, 0, sizeof(*dev));
    dev->bus = config->bus;
    dev->timer = config->timer;
    dev->address = config->address != 0u ? config->address : (rh_u8)ROBOHUB_SHT4X_ADDRESS;
    dev->speed_hz = config->speed_hz != 0u ? config->speed_hz : ROBOHUB_SHT4X_SPEED_HZ;

    dev->temperature.name = "sht4x.temperature";
    dev->temperature.context = dev;
    dev->temperature.init = sht4x_temp_init;
    dev->temperature.deinit = sht4x_temp_deinit;
    dev->temperature.get_celsius = sht4x_temp_celsius;
    dev->temperature.get_fahrenheit = sht4x_temp_fahrenheit;

    dev->humidity.name = "sht4x.humidity";
    dev->humidity.context = dev;
    dev->humidity.init = sht4x_hum_init;
    dev->humidity.deinit = sht4x_hum_deinit;
    dev->humidity.get_humidity = sht4x_hum_get;

    return sht4x_bring_up(dev);
}

RoboHubError robohub_sht4x_deinit(RoboHubSHT4x* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dev->ready = RH_FALSE;
    return ROBOHUB_OK;
}

RoboHubError robohub_sht4x_read(RoboHubSHT4x* dev, rh_f32* celsius, rh_f32* percent)
{
    if (dev == NULL || (celsius == NULL && percent == NULL)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return sht4x_measure(dev, celsius, percent);
}

RoboHubError robohub_sht4x_get_celsius(RoboHubSHT4x* dev, rh_f32* celsius)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return sht4x_temp_celsius(&dev->temperature, celsius);
}

RoboHubError robohub_sht4x_get_humidity(RoboHubSHT4x* dev, rh_f32* percent)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return sht4x_hum_get(&dev->humidity, percent);
}
