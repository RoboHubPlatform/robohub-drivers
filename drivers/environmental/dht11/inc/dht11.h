/**
 * @file dht11.h
 * @brief Driver - the Aosong DHT11 temperature and humidity sensor.
 *
 * A DHT11 measures temperature and relative humidity and reports both in one
 * frame: five bytes on a single data line, humidity integer and decimal,
 * temperature integer and decimal, and a checksum. It is one part presenting two
 * of the driver interfaces `robohub-common` declares, so a program written
 * against `RoboHubTemperature` and one written against `RoboHubHumidity` can both
 * be handed this, and neither names it.
 *
 * It is written against `RoboHubDHT`, the single-wire bus, and not against a pin
 * or a timer of its own. The pulse timing that the wire is made of belongs to
 * the platform, which knows the clock; this file knows only which byte is which
 * and how the checksum is computed. The same object file runs on any board that
 * implements the bus.
 *
 * The device is a value the caller owns, and its first member is the temperature
 * interface:
 *
 *     RoboHubDHT11 sensor;
 *     RoboHubDHT11Config config = {
 *         .bus   = robohub_get_dht("dht0"),
 *         .timer = robohub_get_timer("system_timer"),
 *     };
 *     robohub_dht11_init(&sensor, &config);
 *
 *     float celsius, percent;
 *     sensor.temperature.get_celsius(&sensor.temperature, &celsius);
 *     sensor.humidity.get_humidity(&sensor.humidity, &percent);
 *
 * There is no allocation and nothing to free. Each interface's `context` points
 * back at the device, so a caller may hold either and the driver recovers the
 * device behind it.
 *
 * The part is slow and cannot be sampled faster than about once a second; the
 * driver enforces that when it was given a timer, and leaves it to the caller
 * when it was not.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef DHT11_H
#define DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

/* -------------------------------------------------------------------------- */
/*  Includes                                                                  */
/* -------------------------------------------------------------------------- */
#include "types.h"
#include "error.h"
#include "robohub/hal/dht.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/temperature.h"
#include "robohub/drivers/humidity.h"

/* -------------------------------------------------------------------------- */
/*  Configuration                                                             */
/* -------------------------------------------------------------------------- */

/** The shortest interval between two measurements, in milliseconds. The part
 *  needs about a second to recover, and the DHT11 datasheet asks for at least
 *  that; a DHT22 is the same. */
#define ROBOHUB_DHT11_MIN_INTERVAL_MS 1000u

/**
 * @brief What a DHT11 needs to be usable.
 *
 * The bus is the only mandatory thing. The timer is optional: with one, the
 * driver waits out the part's recovery time between reads, and without one the
 * caller is responsible for not sampling too fast.
 */
typedef struct {
    /** The single-wire bus the part is on. */
    RoboHubDHT* bus;
    /** The timer used to pace measurements. May be NULL. */
    RoboHubTimer* timer;
    /** Minimum interval between measurements, in milliseconds. Zero means 1000. */
    rh_u32 min_interval_ms;
} RoboHubDHT11Config;

/* -------------------------------------------------------------------------- */
/*  Device                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief One DHT11, and the two interfaces it presents.
 *
 * A value, not a handle: the caller declares one and passes its address to
 * `robohub_dht11_init`. The fields after the interfaces are the driver's own and
 * are not part of what a program is written against.
 */
typedef struct {
    /** What a program reads temperature through. Must remain first. */
    RoboHubTemperature temperature;
    /** What a program reads humidity through. */
    RoboHubHumidity    humidity;

    /* Driver state. */
    RoboHubDHT* bus;
    RoboHubTimer* timer;
    rh_u32 min_interval_ms;
    rh_millis last_read_ms;
    rh_bool have_last;
    rh_bool ready;
} RoboHubDHT11;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a DHT11 to a bus and bring it up.
 *
 * Initialises the bus and marks the device ready. Returns `ROBOHUB_ERR_NOT_FOUND`
 * from a read, not from here, when nothing answers — the part answers only once
 * it is asked for a measurement.
 */
RoboHubError robohub_dht11_init(RoboHubDHT11* dev, const RoboHubDHT11Config* config);

/** Take the device down. The bus is left alone; the caller owns it. */
RoboHubError robohub_dht11_deinit(RoboHubDHT11* dev);

/** Read the temperature in degrees Celsius. */
RoboHubError robohub_dht11_get_celsius(RoboHubDHT11* dev, rh_f32* celsius);

/** The same reading in degrees Fahrenheit. */
RoboHubError robohub_dht11_get_fahrenheit(RoboHubDHT11* dev, rh_f32* fahrenheit);

/** Read the relative humidity as a percentage, 0 to 100. */
RoboHubError robohub_dht11_get_humidity(RoboHubDHT11* dev, rh_f32* percent);

/**
 * @brief Read both values from one frame.
 *
 * The part sends temperature and humidity together, so reading both here costs
 * one measurement where reading them through the interfaces in turn would cost
 * two. Either output pointer may be NULL.
 */
RoboHubError robohub_dht11_read(RoboHubDHT11* dev, rh_f32* celsius, rh_f32* percent);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
