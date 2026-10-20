/*
 * bsp.c — Support package for the ky-001-temperature-sensor eval board.
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_001_temperature_sensor_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_001_temperature_sensor_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_001_temperature_sensor_module_init(RoboHubDS18B20* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubOneWire* bus = robohub_get_onewire(RH_KY_001_TEMPERATURE_SENSOR_ONEWIRE_NAME);
    RoboHubTimer* timer = robohub_get_timer(RH_KY_001_TEMPERATURE_SENSOR_TIMER_NAME);
    if (bus == NULL || timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    RoboHubDS18B20Config config = { .bus = bus, .timer = timer };
    return robohub_ds18b20_init(dev, &config);
}
