/*
 * ds18b20.c — Maxim DS18B20 1-Wire temperature sensor.
 *
 * Everything here is protocol and arithmetic. The wire itself — reset pulses,
 * write slots, read slots — belongs to the `RoboHubOneWire` bus a platform
 * implements, and the conversion wait to the `RoboHubTimer`. This file names
 * neither a chip nor a vendor SDK, which is what lets it be compiled for any
 * board that has a 1-Wire bus.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ds18b20.h"
#include "robohub/drivers/temperature_utils.h"

#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Protocol constants                                                        */
/* -------------------------------------------------------------------------- */

/* Every device on the bus, without reading a 64-bit ROM code first. A board
 * with one sensor is the ordinary case, and it is why the driver takes no
 * address. */
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONVERT_T       0x44
#define DS18B20_CMD_WRITE_SCRATCH   0x4E
#define DS18B20_CMD_READ_SCRATCH    0xBE
#define DS18B20_CMD_COPY_SCRATCH    0x48

/* The scratchpad is nine bytes: temperature, alarms, configuration, three
 * reserved bytes, and a CRC over the first eight. */
#define DS18B20_SCRATCHPAD_BYTES    9

/* The conversion time the part needs, in milliseconds, by resolution. Indexed
 * by `resolution_bits - ROBOHUB_DS18B20_RESOLUTION_MIN`. */
static const rh_u16 ds18b20_conversion_ms[4] = { 94u, 188u, 375u, 750u };

/* How long the part takes to copy its scratchpad to EEPROM. */
#define DS18B20_COPY_MS             10u

/* -------------------------------------------------------------------------- */
/*  Helpers                                                                   */
/* -------------------------------------------------------------------------- */

/* The 1-Wire CRC-8: polynomial x^8 + x^5 + x^4 + 1, reflected, seeded zero.
 * Checked rather than trusted because a bus is a wire, and a missed bit in a
 * temperature is a wrong number that looks like a right one. */
static rh_u8 ds18b20_crc8(const rh_u8* data, rh_size len)
{
    rh_u8 crc = 0u;
    for (rh_size i = 0; i < len; i++) {
        rh_u8 byte = data[i];
        for (rh_u8 bit = 0; bit < 8u; bit++) {
            rh_u8 mix = (rh_u8)((crc ^ byte) & 0x01u);
            crc >>= 1;
            if (mix != 0u) {
                crc ^= 0x8Cu;
            }
            byte >>= 1;
        }
    }
    return crc;
}

static RoboHubError ds18b20_write_byte(RoboHubDS18B20* dev, rh_u8 byte)
{
    if (dev->bus->write_byte == NULL) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }
    return dev->bus->write_byte(dev->bus, byte);
}

/* Reset the bus, then SKIP ROM and a command. Every exchange with the part
 * starts this way, which is why it is one function and not five. */
static RoboHubError ds18b20_command(RoboHubDS18B20* dev, rh_u8 command)
{
    if (dev->bus->reset == NULL) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }

    rh_bool present = RH_FALSE;
    RoboHubError err = dev->bus->reset(dev->bus, &present);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    if (!present) {
        return ROBOHUB_ERR_NOT_FOUND;
    }

    err = ds18b20_write_byte(dev, DS18B20_CMD_SKIP_ROM);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    return ds18b20_write_byte(dev, command);
}

static rh_u8 ds18b20_config_byte(rh_u8 resolution_bits)
{
    /* Bits 6:5 select the resolution; the low five bits read as ones. */
    rh_u8 r = (rh_u8)(resolution_bits - ROBOHUB_DS18B20_RESOLUTION_MIN);
    return (rh_u8)((rh_u8)((r & 0x03u) << 5) | 0x1Fu);
}

static rh_u16 ds18b20_conversion_time(rh_u8 resolution_bits)
{
    return ds18b20_conversion_ms[resolution_bits - ROBOHUB_DS18B20_RESOLUTION_MIN];
}

/* Write the resolution and the alarm limits into the scratchpad and commit
 * them to EEPROM, so a part that has been powered off comes back as this
 * device. */
static RoboHubError ds18b20_configure(RoboHubDS18B20* dev)
{
    RoboHubError err = ds18b20_command(dev, DS18B20_CMD_WRITE_SCRATCH);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    err = ds18b20_write_byte(dev, (rh_u8)dev->alarm_high_c);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    err = ds18b20_write_byte(dev, (rh_u8)dev->alarm_low_c);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    err = ds18b20_write_byte(dev, ds18b20_config_byte(dev->resolution_bits));
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    err = ds18b20_command(dev, DS18B20_CMD_COPY_SCRATCH);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    dev->timer->delay_ms(dev->timer, DS18B20_COPY_MS);
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Reading                                                                   */
/* -------------------------------------------------------------------------- */

static RoboHubError ds18b20_read_scratchpad(RoboHubDS18B20* dev)
{
    if (dev->bus->read_byte == NULL) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }

    RoboHubError err = ds18b20_command(dev, DS18B20_CMD_READ_SCRATCH);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    for (rh_size i = 0; i < DS18B20_SCRATCHPAD_BYTES; i++) {
        err = dev->bus->read_byte(dev->bus, &dev->scratchpad[i]);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
    }

    if (ds18b20_crc8(dev->scratchpad, DS18B20_SCRATCHPAD_BYTES - 1u) !=
        dev->scratchpad[DS18B20_SCRATCHPAD_BYTES - 1u]) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                      */
/* -------------------------------------------------------------------------- */

static RoboHubError ds18b20_op_init(RoboHubTemperature* self)
{
    RoboHubDS18B20* dev = (RoboHubDS18B20*)self->context;
    RoboHubError err = ds18b20_configure(dev);
    if (ROBOHUB_SUCCEEDED(err)) {
        dev->ready = RH_TRUE;
    }
    return err;
}

static RoboHubError ds18b20_op_deinit(RoboHubTemperature* self)
{
    RoboHubDS18B20* dev = (RoboHubDS18B20*)self->context;
    dev->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError ds18b20_op_get_celsius(RoboHubTemperature* self, rh_f32* celsius)
{
    RoboHubDS18B20* dev = (RoboHubDS18B20*)self->context;
    if (celsius == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!dev->ready) {
        return ROBOHUB_ERR_STATE;
    }
    if (dev->timer == NULL || dev->timer->delay_ms == NULL) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }

    /* A conversion is one command, a wait and a read, and a bus is a wire: a
     * bit that arrives as noise fails the scratchpad's CRC rather than being
     * read wrong, and the answer is to ask again. Three attempts is enough for
     * the transient an unshielded jumper produces, and a caller still gets the
     * error when the line is genuinely down. */
    RoboHubError err = ROBOHUB_ERR_PROTOCOL;
    for (rh_u32 attempt = 0; attempt < 3u; attempt++) {
        err = ds18b20_command(dev, DS18B20_CMD_CONVERT_T);
        if (ROBOHUB_FAILED(err)) {
            continue;
        }
        dev->timer->delay_ms(dev->timer, ds18b20_conversion_time(dev->resolution_bits));

        err = ds18b20_read_scratchpad(dev);
        if (ROBOHUB_FAILED(err)) {
            continue;
        }

        /* The temperature is a signed 16-bit value in sixteenths of a degree,
         * with the low bits undefined below the configured resolution. */
        rh_i16 raw = (rh_i16)(((rh_u16)dev->scratchpad[1] << 8) | (rh_u16)dev->scratchpad[0]);
        rh_u16 unused_mask = (rh_u16)((1u << (12u - dev->resolution_bits)) - 1u);
        raw = (rh_i16)((rh_u16)raw & (rh_u16)~unused_mask);

        *celsius = (rh_f32)raw * 0.0625f;
        return ROBOHUB_OK;
    }
    return err;
}

static RoboHubError ds18b20_op_get_fahrenheit(RoboHubTemperature* self, rh_f32* fahrenheit)
{
    if (fahrenheit == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rh_f32 celsius = 0.0f;
    RoboHubError err = ds18b20_op_get_celsius(self, &celsius);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *fahrenheit = robohub_celsius_to_fahrenheit(celsius);
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_ds18b20_init(RoboHubDS18B20* dev, const RoboHubDS18B20Config* config)
{
    if (dev == NULL || config == NULL || config->bus == NULL || config->timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config->timer->delay_ms == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    rh_u8 resolution = config->resolution_bits != 0u
        ? config->resolution_bits
        : ROBOHUB_DS18B20_RESOLUTION_MAX;
    if (resolution < ROBOHUB_DS18B20_RESOLUTION_MIN ||
        resolution > ROBOHUB_DS18B20_RESOLUTION_MAX) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->bus = config->bus;
    dev->timer = config->timer;
    dev->resolution_bits = resolution;
    dev->alarm_high_c = config->alarm_high_c != 0 ? config->alarm_high_c : (rh_i8)30;
    dev->alarm_low_c = config->alarm_low_c != 0 ? config->alarm_low_c : (rh_i8)0;
    dev->ready = RH_FALSE;

    dev->interface.name = "ds18b20";
    dev->interface.context = dev;
    dev->interface.init = ds18b20_op_init;
    dev->interface.deinit = ds18b20_op_deinit;
    dev->interface.get_celsius = ds18b20_op_get_celsius;
    dev->interface.get_fahrenheit = ds18b20_op_get_fahrenheit;

    if (dev->bus->init != NULL) {
        RoboHubError err = dev->bus->init(dev->bus);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
    }
    return ds18b20_op_init(&dev->interface);
}

RoboHubError robohub_ds18b20_deinit(RoboHubDS18B20* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dev->ready = RH_FALSE;
    if (dev->bus != NULL && dev->bus->deinit != NULL) {
        return dev->bus->deinit(dev->bus);
    }
    return ROBOHUB_OK;
}

RoboHubError robohub_ds18b20_get_celsius(RoboHubDS18B20* dev, rh_f32* celsius)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return ds18b20_op_get_celsius(&dev->interface, celsius);
}

RoboHubError robohub_ds18b20_get_fahrenheit(RoboHubDS18B20* dev, rh_f32* fahrenheit)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return ds18b20_op_get_fahrenheit(&dev->interface, fahrenheit);
}
