/**
 * @file {{name}}.h
 * @brief Driver interface - {{description}}
 *
 * A driver is written against the HAL interfaces a platform implements and
 * presents the driver interface its kind declares, so the same object file runs
 * on every platform that implements the bus it needs.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef {{name}}_H
#define {{name}}_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "error.h"

/** The device a caller owns. */
typedef struct RoboHub{{category}}Device {
    int placeholder;
} RoboHub{{category}}Device;

/** Configuration: which bus, which timer, how it is wired. */
typedef struct RoboHub{{category}}Config {
    void* bus;
    void* timer;
} RoboHub{{category}}Config;

RoboHubError robohub_{{name}}_init(RoboHub{{category}}Device* self, const RoboHub{{category}}Config* config);

#ifdef __cplusplus
}
#endif

#endif /* {{name}}_H */
