/*
 * bsp.c — Support package for the ky-035-analog-hall-magnetic-sensor eval board.
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_035_analog_hall_magnetic_sensor_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_035_analog_hall_magnetic_sensor_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_035_analog_hall_magnetic_sensor_module_init(RoboHubAnalogAdc* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubADC* adc = robohub_get_adc(RH_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_ADC_NAME);
    if (adc == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_analog_adc_init(dev, adc, (rh_u8)RH_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_CHANNEL,
                RH_KY_035_ANALOG_HALL_MAGNETIC_SENSOR_INVERT ? RH_TRUE : RH_FALSE);
}
