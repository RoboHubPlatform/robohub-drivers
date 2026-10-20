/*
 * gpio_switch.c — an on/off output on a GPIO.
 *
 * The level and the switch are two things, and the inversion is the only place
 * they meet: a switch that is on is the level the wiring calls for, whichever
 * way round that is.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "gpio_switch.h"

#include <stddef.h>

static rh_u32 level_for(const RoboHubGpioSwitch* sw, rh_bool on)
{
    if (sw->active_high) {
        return on ? 1u : 0u;
    }
    return on ? 0u : 1u;
}

static RoboHubError switch_op_init(RoboHubSwitch* self)
{
    RoboHubGpioSwitch* sw = (RoboHubGpioSwitch*)self->context;
    if (sw->gpio == NULL || sw->gpio->set_mode == NULL || sw->gpio->write == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubError err = sw->gpio->set_mode(sw->gpio, sw->pin, ROBOHUB_GPIO_MODE_OUTPUT);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    /* Off at bring-up, so a relay or a buzzer does not act the moment the
     * program starts. */
    err = sw->gpio->write(sw->gpio, sw->pin, level_for(sw, RH_FALSE));
    if (ROBOHUB_SUCCEEDED(err)) {
        sw->ready = RH_TRUE;
    }
    return err;
}

static RoboHubError switch_op_deinit(RoboHubSwitch* self)
{
    RoboHubGpioSwitch* sw = (RoboHubGpioSwitch*)self->context;
    sw->ready = RH_FALSE;
    if (sw->gpio != NULL && sw->gpio->write != NULL) {
        return sw->gpio->write(sw->gpio, sw->pin, level_for(sw, RH_FALSE));
    }
    return ROBOHUB_OK;
}

static RoboHubError switch_op_get(RoboHubSwitch* self, rh_bool* on)
{
    RoboHubGpioSwitch* sw = (RoboHubGpioSwitch*)self->context;
    if (on == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!sw->ready || sw->gpio->read == NULL) {
        return ROBOHUB_ERR_STATE;
    }
    rh_u32 level = 0;
    RoboHubError err = sw->gpio->read(sw->gpio, sw->pin, &level);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *on = sw->active_high ? (level != 0u) : (level == 0u);
    return ROBOHUB_OK;
}

static RoboHubError switch_op_set(RoboHubSwitch* self, rh_bool on)
{
    RoboHubGpioSwitch* sw = (RoboHubGpioSwitch*)self->context;
    if (!sw->ready || sw->gpio->write == NULL) {
        return ROBOHUB_ERR_STATE;
    }
    return sw->gpio->write(sw->gpio, sw->pin, level_for(sw, on));
}

RoboHubError robohub_gpio_switch_init(RoboHubGpioSwitch* sw,
                                      RoboHubGPIO* gpio,
                                      rh_u32 pin,
                                      rh_bool active_high)
{
    if (sw == NULL || gpio == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    sw->gpio = gpio;
    sw->pin = pin;
    sw->active_high = active_high;
    sw->ready = RH_FALSE;

    sw->interface.name = "switch";
    sw->interface.context = sw;
    sw->interface.init = switch_op_init;
    sw->interface.deinit = switch_op_deinit;
    sw->interface.get = switch_op_get;
    sw->interface.set = switch_op_set;

    return switch_op_init(&sw->interface);
}
