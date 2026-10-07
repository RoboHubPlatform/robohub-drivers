/**
 * @file ds18b20.h
 * @brief Driver interface - the Maxim DS18B20 1-Wire temperature sensor.
 *
 * A DS18B20 is one part on one kind of bus, and this is the whole of what a
 * program has to know to use it: a device struct to own, a configuration saying
 * which bus and which timer it is wired to, and the `RoboHubTemperature` it
 * presents. Nothing here names a chip: the bus is a `RoboHubOneWire` a platform
 * implements and the delay is a `RoboHubTimer`, so the same object file runs on
 * an RP2350, an STM32 or an ESP32.
 *
 * The device is a value the caller owns:
 *
 *     RoboHubDS18B20 sensor;
 *     RoboHubDS18B20Config config = {
 *         .bus   = robohub_get_onewire("onewire0"),
 *         .timer = robohub_get_timer("system_timer"),
 *         .resolution_bits = 12,
 *     };
 *     robohub_ds18b20_init(&sensor, &config);
 *     float celsius;
 *     sensor.interface.get_celsius(&sensor.interface, &celsius);
 *
 * There is no allocation and nothing to free. `interface` is the first member
 * so that a caller may hold either the device or the interface it presents and
 * a driver can recover one from the other.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef DS18B20_H
#define DS18B20_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

/* -------------------------------------------------------------------------- */
/*  Includes                                                                  */
/* -------------------------------------------------------------------------- */
#include "types.h"
#include "error.h"
#include "robohub/hal/onewire.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/temperature.h"

/* -------------------------------------------------------------------------- */
/*  Configuration                                                             */
/* -------------------------------------------------------------------------- */

/** The resolutions the part supports, in bits. A conversion is slower the more
 *  of them are asked for: 750 ms at 12 bits, 94 ms at 9. */
#define ROBOHUB_DS18B20_RESOLUTION_MIN 9
#define ROBOHUB_DS18B20_RESOLUTION_MAX 12

/**
 * @brief What a DS18B20 needs to be usable.
 *
 * The bus and the timer are mandatory: without the first there is no device to
 * talk to, and without the second the conversion wait has no way to happen. The
 * alarm limits and resolution are optional and take defined defaults.
 */
typedef struct {
    /** The 1-Wire bus the part is on. */
    RoboHubOneWire* bus;
    /** The timer used to wait out a conversion. */
    RoboHubTimer*   timer;
    /** Conversion resolution in bits, 9 to 12. Zero means 12. */
    rh_u8           resolution_bits;
    /** Upper alarm limit, in whole degrees Celsius. Zero means +30. */
    rh_i8           alarm_high_c;
    /** Lower alarm limit, in whole degrees Celsius. Zero means 0. */
    rh_i8           alarm_low_c;
} RoboHubDS18B20Config;

/* -------------------------------------------------------------------------- */
/*  Device                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief One DS18B20, and the temperature interface it presents.
 *
 * A value, not a handle: the caller declares one and passes its address to
 * `robohub_ds18b20_init`. The fields after `interface` are the driver's own and
 * are not part of what a program is written against.
 */
typedef struct {
    /** What a program is written against. Must remain the first member. */
    RoboHubTemperature interface;

    /* Driver state. */
    RoboHubOneWire* bus;
    RoboHubTimer*   timer;
    rh_u8           resolution_bits;
    rh_i8           alarm_high_c;
    rh_i8           alarm_low_c;
    rh_u8           scratchpad[9];
    rh_bool         ready;
} RoboHubDS18B20;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a DS18B20 to a bus and bring it up.
 *
 * Fills in `dev` and its interface, initialises the bus, checks that a device
 * answers, and writes the resolution and alarm limits to the part. Returns
 * `ROBOHUB_ERR_NOT_FOUND` when nothing answers on the bus, which is the one
 * failure a caller can act on — no sensor is wired, or it is wired wrongly.
 */
RoboHubError robohub_ds18b20_init(RoboHubDS18B20* dev, const RoboHubDS18B20Config* config);

/** Take the device down. The bus is left alone; the platform owns it. */
RoboHubError robohub_ds18b20_deinit(RoboHubDS18B20* dev);

/** Start a conversion and read the result in degrees Celsius. */
RoboHubError robohub_ds18b20_get_celsius(RoboHubDS18B20* dev, rh_f32* celsius);

/** The same reading in degrees Fahrenheit. */
RoboHubError robohub_ds18b20_get_fahrenheit(RoboHubDS18B20* dev, rh_f32* fahrenheit);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_H */
