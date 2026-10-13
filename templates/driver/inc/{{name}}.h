/**
 * @file {{name}}.h
 * @brief Driver - {{description}}
 *
 * A driver is the third of three layers, and the only one written per device:
 *
 *   1. Bus specific — the common register bus, `RoboHubBus`, and its I2C and SPI
 *      backends in robohub-common. This file names none of them.
 *   2. Registry specific — the board's support package, which resolves the
 *      controller by the name the platform registered it under and binds the
 *      bus. This file never sees the registry.
 *   3. Driver — this file. The part's own protocol, written against
 *      `RoboHubBus` and the driver interface it presents, so the same object
 *      file runs on I2C or SPI and on any board.
 *
 * The device is a value the caller owns, and its first member is the interface a
 * program reads it through:
 *
 *     RoboHub{{name}}Device dev;
 *     RoboHub{{name}}Config config = { .bus = bus };
 *     robohub_{{name}}_init(&dev, &config);
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef {{name}}_H
#define {{name}}_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/bus.h"
#include "robohub/hal/timer.h"
/* Include the driver interface this part presents. A part that presents more
 * than one — a thermometer and a barometer, say — includes each and fills in an
 * interface member for every one, and lists each name in the manifest's
 * `functions:`. */
#include "robohub/drivers/temperature.h"

/** Configuration: the bound bus, and a timer when the part needs one. */
typedef struct {
    /** The common register bus the part is on. */
    RoboHubBus* bus;
    /** A timer, when a conversion has to be waited out. May be NULL. */
    RoboHubTimer* timer;
} RoboHub{{name}}Config;

/** The device a caller owns. */
typedef struct {
    /* The interface a program reads the part through. Must remain first; the
     * interface's context points back at this struct. */
    RoboHubTemperature interface;

    /* Driver state. */
    RoboHubBus* bus;
    RoboHubTimer* timer;
    rh_bool ready;
} RoboHub{{name}}Device;

RoboHubError robohub_{{name}}_init(RoboHub{{name}}Device* dev,
                                   const RoboHub{{name}}Config* config);

RoboHubError robohub_{{name}}_deinit(RoboHub{{name}}Device* dev);

#ifdef __cplusplus
}
#endif

#endif /* {{name}}_H */
