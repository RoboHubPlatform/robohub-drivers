/*
 * bsp.c — Support package for the rgb eval board (ky-009-rgb-led).
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_009_rgb_led_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_009_rgb_led_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_009_rgb_led_module_init(RoboHubRgbPwm* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubPWM* pwm = robohub_get_pwm(RH_KY_009_RGB_LED_PWM_NAME);
    if (pwm == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_rgb_pwm_init(dev, pwm, (rh_u8)RH_KY_009_RGB_LED_RED, (rh_u8)RH_KY_009_RGB_LED_GREEN,
                                RH_KY_009_RGB_LED_BLUE_CHANNEL);
}
