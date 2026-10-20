/*
 * bsp.h — Support package for the ky-035-analog-hall-magnetic-sensor eval board.
 *
 * The board's wiring and the defaults it is reached with. It resolves the
 * interface from the registry and hands it to the driver, so an application fits
 * the board without repeating which pin or channel it is on. The wiring is a
 * macro so a board overrides it from the build.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_MODULE_H
#define BOARDS_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_MODULE_H

#ifdef __cplusplus
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/drivers/input/analog_adc.h"

#define RH_BOARD_KY_035_ANALOG_HALL_MAGNETIC_SENSOR 1

/* The wiring, as the carrier exposes it. */
#define RH_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_ADC_NAME "adc0"
#define RH_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_CHANNEL 0u
#define RH_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_INVERT 0

/** The carrier has nothing to bring up on its own. */
void robohub_ky_035_analog_hall_magnetic_sensor_module_bsp_init(void);

/** Resolve the interface from the registry and bind the driver. */
RoboHubError robohub_ky_035_analog_hall_magnetic_sensor_module_init(RoboHubAnalogAdc* dev);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_MODULE_H */
