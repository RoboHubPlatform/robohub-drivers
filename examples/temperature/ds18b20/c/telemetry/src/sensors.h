/**
 * @file sensors.h
 * @brief The node's sensors, as the generated firmware sees them.
 *
 * A node is a set of modules fitted to one board. The board's program does not
 * know which parts those are: Mesh generates this interface and the
 * implementation beside it for the modules the plan says are connected. The
 * default implementation in this checkout fits the DS18B20, so the example
 * still builds and runs on its own.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef RH_NODE_SENSORS_H
#define RH_NODE_SENSORS_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"

/** Bring every fitted sensor up. A missing part is reported, not fatal. */
void sensors_init(void);

/** Read every fitted sensor and publish one SensorReading per signal. */
void sensors_poll(void);

/** The last error a sensor reported, for the diagnostics response. */
RoboHubError sensors_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* RH_NODE_SENSORS_H */
