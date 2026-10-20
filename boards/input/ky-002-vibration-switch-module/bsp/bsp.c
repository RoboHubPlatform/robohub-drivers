/*
 * bsp.c — Support package for the ky-002-vibration-switch eval board.
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_002_vibration_switch_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_002_vibration_switch_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_002_vibration_switch_module_init(RoboHubButtonGpio* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubGPIO* gpio = robohub_get_gpio(RH_KY_002_VIBRATION_SWITCH_GPIO_NAME);
    if (gpio == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_button_gpio_init(dev, gpio, RH_KY_002_VIBRATION_SWITCH_PIN,
                                    RH_KY_002_VIBRATION_SWITCH_ACTIVE_LOW ? RH_TRUE : RH_FALSE);
}
