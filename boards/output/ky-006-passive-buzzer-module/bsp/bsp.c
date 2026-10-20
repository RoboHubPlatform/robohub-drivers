/*
 * bsp.c — Support package for the buzzer eval board (ky-006-passive-buzzer).
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_006_passive_buzzer_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_006_passive_buzzer_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_006_passive_buzzer_module_init(RoboHubTonePwm* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubPWM* pwm = robohub_get_pwm(RH_KY_006_PASSIVE_BUZZER_PWM_NAME);
    if (pwm == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_tone_pwm_init(dev, pwm, (rh_u8)RH_KY_006_PASSIVE_BUZZER_CHANNEL);
}
