/*
 * bsp.c — Support package for the ir_tx eval board (ky-005-ir-transmitter).
 *
 * The join: the registry holds the platform's interface, the driver holds the
 * part, and this looks the interface up by the name the board was given and
 * hands it to the driver.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_005_ir_transmitter_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_005_ir_transmitter_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_005_ir_transmitter_module_init(RoboHubInfraredPwm* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubPWM* pwm = robohub_get_pwm(RH_KY_005_IR_TRANSMITTER_PWM_NAME);
    RoboHubTimer* timer = robohub_get_timer(RH_KY_005_IR_TRANSMITTER_TIMER_NAME);
    if (pwm == NULL || timer == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_infrared_pwm_init(dev, pwm, timer, (rh_u8)RH_KY_005_IR_TRANSMITTER_CHANNEL);
}
