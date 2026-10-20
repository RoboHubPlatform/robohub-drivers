/*
 * bsp.c — Support package for the ky-023-joystick-dual-axis eval board.
 *
 * The join, once per device the board carries.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "ky_023_joystick_dual_axis_module.h"
#include "registry.h"

#include <stddef.h>

void robohub_ky_023_joystick_dual_axis_module_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up. */
}

RoboHubError robohub_ky_023_joystick_dual_axis_module_init_x(RoboHubAnalogAdc* axis)
{
    if (axis == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubADC* adc = robohub_get_adc(RH_KY_023_JOYSTICK_ADC_NAME);
    if (adc == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_analog_adc_init(axis, adc, (rh_u8)RH_KY_023_JOYSTICK_X_CHANNEL, RH_FALSE);
}

RoboHubError robohub_ky_023_joystick_dual_axis_module_init_y(RoboHubAnalogAdc* axis)
{
    if (axis == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubADC* adc = robohub_get_adc(RH_KY_023_JOYSTICK_ADC_NAME);
    if (adc == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_analog_adc_init(axis, adc, (rh_u8)RH_KY_023_JOYSTICK_Y_CHANNEL, RH_FALSE);
}

RoboHubError robohub_ky_023_joystick_dual_axis_module_init_switch(RoboHubButtonGpio* press)
{
    if (press == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubGPIO* gpio = robohub_get_gpio(RH_KY_023_JOYSTICK_GPIO_NAME);
    if (gpio == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return robohub_button_gpio_init(press, gpio, RH_KY_023_JOYSTICK_SWITCH_PIN,
                                   RH_KY_023_JOYSTICK_SWITCH_ACTIVE_LOW ? RH_TRUE : RH_FALSE);
}
