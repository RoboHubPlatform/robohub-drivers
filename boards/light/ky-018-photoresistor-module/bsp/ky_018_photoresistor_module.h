/*
 * bsp.h — Support package for the photoresistor eval board (ky-018-photoresistor).
 *
 * The board's wiring and the defaults it is reached with. It resolves the
 * interface from the registry and hands it to the driver, so an application
 * fits the board without repeating which pin or channel it is on. The wiring is
 * a macro so a board overrides it from the build.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_KY_018_PHOTORESISTOR_BSP_BSP_H
#define BOARDS_KY_018_PHOTORESISTOR_BSP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/drivers/light/photoresistor_adc.h"
#include "robohub/hal/adc.h"

#define RH_BOARD_KY_018_PHOTORESISTOR 1

/* The wiring, as the carrier exposes it. */
#define RH_KY_018_PHOTORESISTOR_ADC_NAME "adc0"
#define RH_KY_018_PHOTORESISTOR_CHANNEL 0u
#define RH_KY_018_PHOTORESISTOR_INVERT 1

/** The carrier has nothing to bring up on its own. */
void robohub_ky_018_photoresistor_module_bsp_init(void);

/** Resolve the interface from the registry and bind the driver. */
RoboHubError robohub_ky_018_photoresistor_module_init(RoboHubPhotoresistorAdc* dev);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_KY_018_PHOTORESISTOR_BSP_BSP_H */
