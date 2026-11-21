/**
 * @file sht4x.h
 * @brief Driver - Sensirion's SHT4x temperature and humidity sensors.
 *
 * An SHT40, SHT41 or SHT45 measures temperature and relative humidity and
 * answers on I2C with six bytes: the temperature word, its CRC-8, the humidity
 * word, its CRC-8 (Sensirion SHT4x datasheet). It is one part presenting two of
 * the driver interfaces robohub-common declares, so a program written against
 * `RoboHubTemperature` and one written against `RoboHubHumidity` can both be
 * handed this, and neither names it (DRV-ENV-0007, robohub ADR-0026).
 *
 * It is written against `RoboHubI2C` and a `RoboHubTimer`, not a chip or a pin:
 * the bus is the platform's, and this file knows only the commands, the CRC and
 * the conversion.
 *
 *     RoboHubSHT4x sensor;
 *     RoboHubSHT4xConfig config = {
 *         .bus     = robohub_get_i2c("i2c0"),
 *         .timer   = robohub_get_timer("system_timer"),
 *         .address = ROBOHUB_SHT4X_ADDRESS,
 *     };
 *     robohub_sht4x_init(&sensor, &config);
 *
 *     float celsius, percent;
 *     robohub_sht4x_read(&sensor, &celsius, &percent);
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef SHT4X_H
#define SHT4X_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/i2c.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/temperature.h"
#include "robohub/drivers/humidity.h"

/* -------------------------------------------------------------------------- */
/*  The part                                                                  */
/* -------------------------------------------------------------------------- */

/** The address of an SHT40/41/45-AD1B, the common part. -BD1B is 0x45. */
#define ROBOHUB_SHT4X_ADDRESS          0x44u
/** Measure temperature and humidity with high precision. */
#define ROBOHUB_SHT4X_CMD_MEASURE_HIGH 0xFDu
/** Read the part's 32-bit serial number. */
#define ROBOHUB_SHT4X_CMD_SERIAL       0x89u
/** Soft reset. */
#define ROBOHUB_SHT4X_CMD_SOFT_RESET   0x94u
/** The longest a high-precision measurement takes (datasheet: 8.3 ms). */
#define ROBOHUB_SHT4X_MEASURE_MS       9u
/** The longest a soft reset takes (datasheet: 1 ms). */
#define ROBOHUB_SHT4X_RESET_MS         2u
/** The bus speed the part is driven at: standard mode. */
#define ROBOHUB_SHT4X_SPEED_HZ         100000u

typedef struct {
    /** The I2C bus the part is on. */
    RoboHubI2C*   bus;
    /** The timer that waits out a measurement. Required. */
    RoboHubTimer* timer;
    /** The part's 7-bit address. Zero means 0x44. */
    rh_u8         address;
    /** The bus speed. Zero means 100 kHz. */
    rh_u32        speed_hz;
} RoboHubSHT4xConfig;

/* -------------------------------------------------------------------------- */
/*  Device                                                                    */
/* -------------------------------------------------------------------------- */

typedef struct {
    /** What a program reads temperature through. Must remain first. */
    RoboHubTemperature temperature;
    /** What a program reads humidity through. */
    RoboHubHumidity    humidity;

    /* Driver state. */
    RoboHubI2C*   bus;
    RoboHubTimer* timer;
    rh_u8         address;
    rh_u32        speed_hz;
    /** The part's serial number, read at initialisation. */
    rh_u32        serial;
    rh_bool       ready;
    /** A read failed: the part is reset before the next measurement. */
    rh_bool       needs_reset;
} RoboHubSHT4x;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

/** Bring the part up: reset it and read its serial number. */
RoboHubError robohub_sht4x_init(RoboHubSHT4x* dev, const RoboHubSHT4xConfig* config);
RoboHubError robohub_sht4x_deinit(RoboHubSHT4x* dev);
/** One measurement: either pointer may be NULL, not both. */
RoboHubError robohub_sht4x_read(RoboHubSHT4x* dev, rh_f32* celsius, rh_f32* percent);
RoboHubError robohub_sht4x_get_celsius(RoboHubSHT4x* dev, rh_f32* celsius);
RoboHubError robohub_sht4x_get_humidity(RoboHubSHT4x* dev, rh_f32* percent);

/** The CRC-8 Sensirion puts after each word: polynomial 0x31, init 0xFF. */
rh_u8 robohub_sht4x_crc8(const rh_u8* data, rh_size length);
/** The datasheet's conversions of a raw word. */
rh_f32 robohub_sht4x_celsius(rh_u16 raw);
rh_f32 robohub_sht4x_percent(rh_u16 raw);

#ifdef __cplusplus
}
#endif

#endif /* SHT4X_H */
