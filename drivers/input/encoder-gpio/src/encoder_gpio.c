/*
 * encoder_gpio.c — a rotary encoder on two GPIOs.
 *
 * Two signals whose order says which way: the state of the two pins together is
 * one of four, and the move from one state to the next is a step one way or the
 * other. The table is what says which, and it is the same table for every
 * encoder that counts the same way.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "encoder_gpio.h"

#include <stddef.h>

/* The step a move from one state to the next is: the index is the old state
 * times four plus the new one, and the value is the step, positive one way. */
static const rh_i8 encoder_steps[16] = {
    0, -1, 1, 0,
    1,  0, 0, -1,
   -1,  0, 0, 1,
    0,  1, -1, 0
};

static RoboHubError encoder_op_init(RoboHubEncoder* self)
{
    RoboHubEncoderGpio* encoder = (RoboHubEncoderGpio*)self->context;
    if (encoder->gpio == NULL || encoder->gpio->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (encoder->gpio->set_mode != NULL) {
        (void)encoder->gpio->set_mode(encoder->gpio, encoder->pin_a, ROBOHUB_GPIO_MODE_INPUT);
        (void)encoder->gpio->set_mode(encoder->gpio, encoder->pin_b, ROBOHUB_GPIO_MODE_INPUT);
    }
    encoder->steps = 0;
    encoder->last_state = 0u;
    encoder->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError encoder_op_deinit(RoboHubEncoder* self)
{
    ((RoboHubEncoderGpio*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

/* The two pins together, as one of four states. */
static RoboHubError encoder_state(RoboHubEncoderGpio* encoder, rh_u8* state)
{
    rh_u32 a = 0;
    rh_u32 b = 0;
    RoboHubError err = encoder->gpio->read(encoder->gpio, encoder->pin_a, &a);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    err = encoder->gpio->read(encoder->gpio, encoder->pin_b, &b);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    rh_u8 value = (rh_u8)(((a != 0u) ? 2u : 0u) | ((b != 0u) ? 1u : 0u));
    if (encoder->invert) {
        value = (rh_u8)(value ^ 3u);
    }
    *state = value;
    return ROBOHUB_OK;
}

static RoboHubError encoder_op_read(RoboHubEncoder* self, rh_i32* steps, rh_bool* clockwise)
{
    RoboHubEncoderGpio* encoder = (RoboHubEncoderGpio*)self->context;
    if (steps == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!encoder->ready) {
        return ROBOHUB_ERR_STATE;
    }

    rh_u8 state = 0u;
    RoboHubError err = encoder_state(encoder, &state);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    if (state != encoder->last_state) {
        rh_u8 index = (rh_u8)((encoder->last_state << 2) | state);
        encoder->steps += (rh_i32)encoder_steps[index & 0x0Fu];
        encoder->last_state = state;
    }

    *steps = encoder->steps;
    if (clockwise != NULL) {
        *clockwise = encoder->steps >= 0 ? RH_TRUE : RH_FALSE;
    }
    encoder->steps = 0;
    return ROBOHUB_OK;
}

RoboHubError robohub_encoder_gpio_init(RoboHubEncoderGpio* encoder,
                                       RoboHubGPIO* gpio,
                                       rh_u32 pin_a,
                                       rh_u32 pin_b,
                                       rh_bool invert)
{
    if (encoder == NULL || gpio == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    encoder->gpio = gpio;
    encoder->pin_a = pin_a;
    encoder->pin_b = pin_b;
    encoder->invert = invert;
    encoder->steps = 0;
    encoder->last_state = 0u;
    encoder->ready = RH_FALSE;

    encoder->interface.name = "encoder";
    encoder->interface.context = encoder;
    encoder->interface.init = encoder_op_init;
    encoder->interface.deinit = encoder_op_deinit;
    encoder->interface.read = encoder_op_read;

    return encoder_op_init(&encoder->interface);
}
