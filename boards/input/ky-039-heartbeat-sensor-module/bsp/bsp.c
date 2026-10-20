/*
 * bsp.c — Support package for the ky-039-heartbeat-sensor eval board.
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_039_heartbeat_sensor_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_039_heartbeat_sensor_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_039_heartbeat_sensor_module_init(RoboHubPulseAdc* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubADC* adc = robohub_get_adc(RH_KY_039_HEARTBEAT_SENSOR_ADC_NAME);
    RoboHubTimer* timer = robohub_get_timer(RH_KY_039_HEARTBEAT_SENSOR_TIMER_NAME);
    if (adc == NULL || timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_pulse_adc_init(dev, adc, timer, (rh_u8)RH_KY_039_HEARTBEAT_SENSOR_CHANNEL,
                                  RH_KY_039_HEARTBEAT_SENSOR_THRESHOLD);
}
