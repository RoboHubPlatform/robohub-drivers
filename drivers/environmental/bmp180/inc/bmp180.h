/**
 * @file bmp180.h
 * @brief Driver - the Bosch BMP180 temperature, pressure and altitude sensor.
 *
 * A BMP180 measures two things — temperature and pressure — and a third,
 * altitude, is computed from the pressure against a reference. It is one part on
 * one bus presenting three of the driver interfaces `robohub-common` declares,
 * which is why it is filed as a mixed sensor: a program that wants a temperature
 * sensor, one that wants a barometer and one that wants an altimeter are all
 * handed this, and none of them names it.
 *
 * It is written against `RoboHubBus`, the common register bus, and not against
 * I2C or SPI. The bus the caller binds may be either, and nothing here changes
 * when it is: the register sequence and the arithmetic are the same on both
 * wires, which is the whole reason the common bus exists. The same object file
 * runs on an RP2350, an STM32 or an ESP32 because each platform implements the
 * bus, not because this file knows about any of them.
 *
 * The device is a value the caller owns, and its first member is the temperature
 * interface:
 *
 *     RoboHubBusI2C bus;
 *     robohub_bus_i2c_open(&bus, "i2c0", 0x77, 400000);
 *
 *     RoboHubBMP180 sensor;
 *     RoboHubBMP180Config config = { .bus = &bus.bus, .oversampling = 1 };
 *     robohub_bmp180_init(&sensor, &config);
 *
 *     float celsius, pascals, metres;
 *     sensor.temperature.get_celsius(&sensor.temperature, &celsius);
 *     sensor.pressure.get_pressure(&sensor.pressure, &pascals);
 *     sensor.altitude.get_altitude(&sensor.altitude, &metres);
 *
 * There is no allocation and nothing to free. Each interface's `context` points
 * back at the device, so a caller may hold any of the three and the driver
 * recovers the device behind it.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BMP180_H
#define BMP180_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

/* -------------------------------------------------------------------------- */
/*  Includes                                                                  */
/* -------------------------------------------------------------------------- */
#include "types.h"
#include "error.h"
#include "robohub/hal/bus.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/temperature.h"
#include "robohub/drivers/pressure.h"
#include "robohub/drivers/altitude.h"

/* -------------------------------------------------------------------------- */
/*  Identity                                                                  */
/* -------------------------------------------------------------------------- */

/** The 7-bit I2C address the part answers to when its SDO pin is low. */
#define ROBOHUB_BMP180_I2C_ADDRESS 0x77

/** The value the chip-id register reads back. */
#define ROBOHUB_BMP180_CHIP_ID 0x55

/** Oversampling settings, the number of extra samples a pressure reading takes.
 *  More of them is a quieter reading and a longer conversion. */
#define ROBOHUB_BMP180_OSS_MIN 0u
#define ROBOHUB_BMP180_OSS_MAX 3u

/* -------------------------------------------------------------------------- */
/*  Configuration                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief What a BMP180 needs to be usable.
 *
 * The bus is the only mandatory thing: one bound `RoboHubBus`, I2C or SPI. The
 * oversampling and the altitude reference are optional and take defined
 * defaults.
 */
typedef struct {
    /** The common bus the part is on, already bound. */
    RoboHubBus* bus;
    /** The timer used to wait out a conversion. Optional; without one the part's
     *  end-of-conversion bit is polled instead. */
    RoboHubTimer* timer;
    /** Pressure oversampling, 0 to 3. Zero means 0. */
    rh_u8       oversampling;
    /** Sea-level reference for altitude, in pascals. Zero means 101325. */
    rh_f32      sea_level_pa;
} RoboHubBMP180Config;

/* -------------------------------------------------------------------------- */
/*  Device                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief One BMP180, and the three interfaces it presents.
 *
 * A value, not a handle: the caller declares one and passes its address to
 * `robohub_bmp180_init`. The calibration coefficients are read from the part at
 * init and kept here, because the pressure arithmetic needs all of them.
 */
typedef struct {
    /** What a program reads temperature through. Must remain first. */
    RoboHubTemperature temperature;
    /** What a program reads pressure through. */
    RoboHubPressure    pressure;
    /** What a program reads altitude through. */
    RoboHubAltitude    altitude;

    /* Driver state. */
    RoboHubBus* bus;
    RoboHubTimer* timer;
    rh_u8       oversampling;
    rh_f32      sea_level_pa;

    /* Factory calibration, read once at init. */
    rh_i16 ac1;
    rh_i16 ac2;
    rh_i16 ac3;
    rh_i16 ac4;
    rh_i16 ac5;
    rh_i16 ac6;
    rh_i16 b1;
    rh_i16 b2;
    rh_i16 mb;
    rh_i16 mc;
    rh_i16 md;

    rh_bool ready;
} RoboHubBMP180;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a BMP180 to a bus and bring it up.
 *
 * Reads the factory calibration and checks the chip id. Returns
 * `ROBOHUB_ERR_NOT_FOUND` when the part does not answer, which is the one
 * failure a caller can act on — nothing is wired, or it is wired wrongly — and
 * `ROBOHUB_ERR_PROTOCOL` when it answers with the wrong identity.
 */
RoboHubError robohub_bmp180_init(RoboHubBMP180* dev, const RoboHubBMP180Config* config);

/** Take the device down. The bus is left alone; the caller owns it. */
RoboHubError robohub_bmp180_deinit(RoboHubBMP180* dev);

/** Read the temperature in degrees Celsius. */
RoboHubError robohub_bmp180_get_celsius(RoboHubBMP180* dev, rh_f32* celsius);

/** Read the pressure in pascals. */
RoboHubError robohub_bmp180_get_pressure(RoboHubBMP180* dev, rh_f32* pascals);

/**
 * @brief Read the altitude in metres above the configured reference.
 *
 * The pressure is read again to answer this, so a caller that wants altitude and
 * pressure together is reading the part twice. That is the cost of the three
 * interfaces being independent: a program holding only the altitude interface is
 * not made to know about the pressure one.
 */
RoboHubError robohub_bmp180_get_altitude(RoboHubBMP180* dev, rh_f32* metres);

/** Set the sea-level pressure altitude is measured against, in pascals. */
RoboHubError robohub_bmp180_set_sea_level(RoboHubBMP180* dev, rh_f32 sea_level_pa);

/**
 * @brief Read any of the three values in one pass over the part.
 *
 * Any output pointer may be NULL; a NULL is a value the caller does not want.
 * The temperature and the pressure conversions are each issued once, so reading
 * all three here is cheaper than calling the three interfaces in turn — the
 * price of the interfaces being independent is that each reads on its own.
 */
RoboHubError robohub_bmp180_read(RoboHubBMP180* dev,
                                 rh_f32* celsius,
                                 rh_f32* pascals,
                                 rh_f32* metres);

#ifdef __cplusplus
}
#endif

#endif /* BMP180_H */
