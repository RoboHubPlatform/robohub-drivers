/*
 * infrared_pwm.c — an infrared LED on a PWM channel.
 *
 * A mark is the carrier at half duty; a space is the channel stopped. The timer
 * says how long each lasts. The sequence is the protocol's and this only drives
 * it, so a code is data and not code.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "infrared_pwm.h"

#include <stddef.h>

static RoboHubError ir_op_init(RoboHubInfrared* self)
{
    RoboHubInfraredPwm* ir = (RoboHubInfraredPwm*)self->context;
    if (ir->pwm == NULL || ir->pwm->set_duty == NULL || ir->pwm->set_frequency == NULL ||
        ir->timer == NULL || ir->timer->delay_us == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    ir->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError ir_op_deinit(RoboHubInfrared* self)
{
    RoboHubInfraredPwm* ir = (RoboHubInfraredPwm*)self->context;
    ir->ready = RH_FALSE;
    if (ir->pwm->stop != NULL) {
        return ir->pwm->stop(ir->pwm, ir->channel);
    }
    return ROBOHUB_OK;
}

static rh_u32 ir_op_carrier(RoboHubInfrared* self)
{
    return ((RoboHubInfraredPwm*)self->context)->carrier;
}

/* Gate the carrier: a mark drives it, a space silences it. */
static RoboHubError ir_gate(RoboHubInfraredPwm* ir, rh_bool on)
{
    if (on) {
        return ir->pwm->set_duty(ir->pwm, ir->channel, 0.5f);
    }
    if (ir->pwm->stop != NULL) {
        return ir->pwm->stop(ir->pwm, ir->channel);
    }
    return ir->pwm->set_duty(ir->pwm, ir->channel, 0.0f);
}

static RoboHubError ir_op_send(RoboHubInfrared* self, const rh_u32* durations, rh_size count)
{
    RoboHubInfraredPwm* ir = (RoboHubInfraredPwm*)self->context;
    if (!ir->ready) {
        return ROBOHUB_ERR_STATE;
    }
    if (durations == NULL || count == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    RoboHubError err = ir->pwm->set_frequency(ir->pwm, ir->carrier);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    for (rh_size i = 0; i < count; i++) {
        /* Even indices are marks, odd ones spaces, starting with a mark. */
        err = ir_gate(ir, (i % 2u) == 0u);
        if (ROBOHUB_FAILED(err)) {
            (void)ir_gate(ir, RH_FALSE);
            return err;
        }
        ir->timer->delay_us(ir->timer, durations[i]);
    }
    return ir_gate(ir, RH_FALSE);
}

RoboHubError robohub_infrared_pwm_init(RoboHubInfraredPwm* ir,
                                       RoboHubPWM* pwm,
                                       RoboHubTimer* timer,
                                       rh_u8 channel)
{
    if (ir == NULL || pwm == NULL || timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    ir->pwm = pwm;
    ir->timer = timer;
    ir->channel = channel;
    ir->carrier = ROBOHUB_INFRARED_CARRIER_HZ;
    ir->ready = RH_FALSE;

    ir->interface.name = "ir_tx";
    ir->interface.context = ir;
    ir->interface.init = ir_op_init;
    ir->interface.deinit = ir_op_deinit;
    ir->interface.carrier_hz = ir_op_carrier;
    ir->interface.send = ir_op_send;

    return ir_op_init(&ir->interface);
}
