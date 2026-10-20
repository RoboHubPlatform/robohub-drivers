/*
 * button_gpio.c — a discrete contact on a GPIO.
 *
 * The level and the contact are two things, and the inversion is the only place
 * they meet: a contact that is closed is pressed, whichever level closes it.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "button_gpio.h"

#include <stddef.h>

static RoboHubError button_op_init(RoboHubButton* self)
{
    RoboHubButtonGpio* button = (RoboHubButtonGpio*)self->context;
    if (button->gpio == NULL || button->gpio->set_mode == NULL || button->gpio->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubError err = button->gpio->set_mode(button->gpio, button->pin, ROBOHUB_GPIO_MODE_INPUT);
    if (ROBOHUB_SUCCEEDED(err)) {
        button->ready = RH_TRUE;
    }
    return err;
}

static RoboHubError button_op_deinit(RoboHubButton* self)
{
    ((RoboHubButtonGpio*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError button_op_get(RoboHubButton* self, rh_bool* pressed)
{
    RoboHubButtonGpio* button = (RoboHubButtonGpio*)self->context;
    if (pressed == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!button->ready) {
        return ROBOHUB_ERR_STATE;
    }
    rh_u32 level = 0;
    RoboHubError err = button->gpio->read(button->gpio, button->pin, &level);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *pressed = button->active_low ? (level == 0u) : (level != 0u);
    return ROBOHUB_OK;
}

RoboHubError robohub_button_gpio_init(RoboHubButtonGpio* button,
                                      RoboHubGPIO* gpio,
                                      rh_u32 pin,
                                      rh_bool active_low)
{
    if (button == NULL || gpio == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    button->gpio = gpio;
    button->pin = pin;
    button->active_low = active_low;
    button->ready = RH_FALSE;

    button->interface.name = "button";
    button->interface.context = button;
    button->interface.init = button_op_init;
    button->interface.deinit = button_op_deinit;
    button->interface.get = button_op_get;

    return button_op_init(&button->interface);
}
