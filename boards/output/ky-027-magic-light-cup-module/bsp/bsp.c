/*
 * bsp.c — Support package for the ky-027-magic-light-cup eval board.
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_027_magic_light_cup_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_027_magic_light_cup_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_027_magic_light_cup_module_init(RoboHubGpioSwitch* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubGPIO* gpio = robohub_get_gpio(RH_KY_027_MAGIC_LIGHT_CUP_GPIO_NAME);
    if (gpio == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_gpio_switch_init(dev, gpio, RH_KY_027_MAGIC_LIGHT_CUP_PIN,
                                    RH_KY_027_MAGIC_LIGHT_CUP_ACTIVE_HIGH ? RH_TRUE : RH_FALSE);
}
