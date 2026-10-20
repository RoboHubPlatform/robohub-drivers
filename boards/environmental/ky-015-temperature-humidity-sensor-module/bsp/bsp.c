/*
 * bsp.c — Support package for the ky-015-temperature-humidity-sensor eval board.
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_015_temperature_humidity_sensor_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_015_temperature_humidity_sensor_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_015_temperature_humidity_sensor_module_init(RoboHubDHT11* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubDHT* bus = robohub_get_dht(RH_KY_015_TEMPERATURE_HUMIDITY_SENSOR_DHT_NAME);
    RoboHubTimer* timer = robohub_get_timer(RH_KY_015_TEMPERATURE_HUMIDITY_SENSOR_TIMER_NAME);
    if (bus == NULL || timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    RoboHubDHT11Config config = { .bus = bus, .timer = timer };
    return robohub_dht11_init(dev, &config);
}
