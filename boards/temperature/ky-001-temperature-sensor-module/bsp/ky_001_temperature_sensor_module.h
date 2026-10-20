/*
 * bsp.h — Support package for the ky-001-temperature-sensor eval board.
 *
 * The board's wiring and the defaults it is reached with. It resolves the
 * interface from the registry and hands it to the driver, so an application fits
 * the board without repeating which pin or channel it is on. The wiring is a
 * macro so a board overrides it from the build.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_KY_001_TEMPERATURE_SENSOR_MODULE_H
#define BOARDS_KY_001_TEMPERATURE_SENSOR_MODULE_H

#ifdef __cplusplus
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/drivers/temperature/ds18b20.h"

#define RH_BOARD_KY_001_TEMPERATURE_SENSOR 1

/* The wiring, as the carrier exposes it. */
#define RH_KY_001_TEMPERATURE_SENSOR_ONEWIRE_NAME "onewire0"
#define RH_KY_001_TEMPERATURE_SENSOR_TIMER_NAME "system_timer"

/** The carrier has nothing to bring up on its own. */
void robohub_ky_001_temperature_sensor_module_bsp_init(void);

/** Resolve the interface from the registry and bind the driver. */
RoboHubError robohub_ky_001_temperature_sensor_module_init(RoboHubDS18B20* dev);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_KY_001_TEMPERATURE_SENSOR_MODULE_H */
