/*
 * infrared_receiver_gpio.c — an infrared receiver on a GPIO.
 *
 * What reaches the pin is a burst of pulses, and a protocol is what gives it a
 * meaning. NEC is decoded here: a header mark of about nine milliseconds, a gap
 * of about four and a half, then thirty-two bits — a mark of about 560
 * microseconds followed by a gap of about 1125 for a zero or about 2250 for a
 * one — and then the same thirty-two bits inverted, which is how a code is
 * checked rather than merely read.
 *
 * The pulses are timed by polling the pin. The shortest thing that has to be
 * told apart is a mark against a gap, and the gaps differ by about a
 * millisecond, so a poll of tens of microseconds is enough.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "infrared_receiver_gpio.h"

#include <stddef.h>

/* The NEC timings, in microseconds. The window each one is allowed to fall in
 * is what tells a mark from a gap and a zero from a one. */
#define NEC_HEADER_MARK_US      9000u
#define NEC_HEADER_GAP_US       4500u
#define NEC_BIT_MARK_US          560u
#define NEC_ZERO_GAP_US         1125u
#define NEC_ONE_GAP_US          2250u
/* How far a timing may be from the one it is compared with. */
#define NEC_TOLERANCE_US         400u
/* The longest a gap may be and still be a gap rather than the end of a burst. */
#define NEC_MAX_GAP_US          8000u
/* How long to wait for the first mark before deciding nothing is arriving. */
#define NEC_IDLE_TIMEOUT_US    20000u

/* How many edges a burst may have. A NEC frame is a header and thirty-three
 * bits, which is sixty-eight edges. */
#define NEC_MAX_EDGES           72u

/* The line is idle at this level; a mark pulls it the other way. */
static rh_bool ir_line(RoboHubInfraredReceiverGpio* ir)
{
    rh_u32 level = 0;
    if (ir->gpio->read(ir->gpio, ir->pin, &level) != ROBOHUB_OK) {
        return RH_FALSE;
    }
    if (ir->active_low) {
        return level == 0u ? RH_TRUE : RH_FALSE;
    }
    return level != 0u ? RH_TRUE : RH_FALSE;
}

/* Time the gaps between the edges of one burst. */
static rh_size ir_capture(RoboHubInfraredReceiverGpio* ir, rh_u32* gaps, rh_size max)
{
    rh_size count = 0u;
    rh_micros previous = ir->timer->get_micros(ir->timer);
    rh_micros last_edge = previous;

    while (count < max) {
        rh_micros now = ir->timer->get_micros(ir->timer);
        if (ir_line(ir) != RH_TRUE) {
            /* An edge: the gap that just ended is the time since the last one. */
            rh_u32 gap = (rh_u32)(now - previous);
            gaps[count++] = gap;
            previous = now;
            last_edge = now;
        } else if ((now - last_edge) > NEC_IDLE_TIMEOUT_US) {
            break; /* nothing more is coming */
        }
    }
    return count;
}

/* Is a timing close enough to the one it is compared with? */
static rh_bool near(rh_u32 measured, rh_u32 expected)
{
    rh_u32 difference = measured > expected ? measured - expected : expected - measured;
    return difference <= NEC_TOLERANCE_US ? RH_TRUE : RH_FALSE;
}

/* Decode a captured burst as NEC, into the code. */
static rh_bool ir_decode(const rh_u32* gaps, rh_size count, rh_u32* code)
{
    if (count < 3u) {
        return RH_FALSE;
    }
    /* The first gap is the header mark, the second the gap after it. */
    if (!near(gaps[0], NEC_HEADER_MARK_US) || !near(gaps[1], NEC_HEADER_GAP_US)) {
        return RH_FALSE;
    }
    /* Then thirty-two bits, each a mark and a gap, and the inverted copy. */
    if (count < 2u + 2u * 32u + 1u) {
        return RH_FALSE;
    }
    rh_u32 value = 0u;
    rh_u32 inverted = 0u;
    for (rh_u32 bit = 0; bit < 32u; bit++) {
        rh_u32 mark = gaps[2u + 2u * bit];
        rh_u32 gap = gaps[3u + 2u * bit];
        if (!near(mark, NEC_BIT_MARK_US)) {
            return RH_FALSE;
        }
        rh_u32 one = 0u;
        if (near(gap, NEC_ONE_GAP_US)) {
            one = 1u;
        } else if (!near(gap, NEC_ZERO_GAP_US)) {
            return RH_FALSE;
        }
        if (bit < 16u) {
            value |= one << bit;
        } else {
            inverted |= one << (bit - 16u);
        }
    }
    /* The second half is the first half inverted, which is what a correct
     * remote sends and a false one does not. */
    rh_u32 complement = (~value) & 0xFFFFu;
    if (complement != inverted) {
        return RH_FALSE;
    }
    *code = value;
    return RH_TRUE;
}

static RoboHubError ir_op_init(RoboHubInfraredReceiver* self)
{
    RoboHubInfraredReceiverGpio* ir = (RoboHubInfraredReceiverGpio*)self->context;
    if (ir->gpio == NULL || ir->gpio->read == NULL || ir->timer == NULL ||
        ir->timer->get_micros == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (ir->gpio->set_mode != NULL) {
        (void)ir->gpio->set_mode(ir->gpio, ir->pin, ROBOHUB_GPIO_MODE_INPUT);
    }
    ir->code = 0u;
    ir->have_code = RH_FALSE;
    ir->protocol = ROBOHUB_INFRARED_PROTOCOL_NONE;
    ir->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError ir_op_deinit(RoboHubInfraredReceiver* self)
{
    ((RoboHubInfraredReceiverGpio*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError ir_op_read(RoboHubInfraredReceiver* self, rh_u32* code, rh_bool* received)
{
    RoboHubInfraredReceiverGpio* ir = (RoboHubInfraredReceiverGpio*)self->context;
    if (code == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!ir->ready) {
        return ROBOHUB_ERR_STATE;
    }

    rh_u32 gaps[NEC_MAX_EDGES];
    rh_size count = ir_capture(ir, gaps, NEC_MAX_EDGES);
    ir->have_code = RH_FALSE;
    ir->protocol = ROBOHUB_INFRARED_PROTOCOL_NONE;
    *code = 0u;

    rh_u32 decoded = 0u;
    if (ir_decode(gaps, count, &decoded)) {
        ir->code = decoded;
        ir->have_code = RH_TRUE;
        ir->protocol = ROBOHUB_INFRARED_PROTOCOL_NEC;
        *code = decoded;
    }
    if (received != NULL) {
        *received = ir->have_code;
    }
    return ROBOHUB_OK;
}

static rh_u8 ir_op_protocol(RoboHubInfraredReceiver* self)
{
    return ((RoboHubInfraredReceiverGpio*)self->context)->protocol;
}

RoboHubError robohub_infrared_receiver_gpio_init(RoboHubInfraredReceiverGpio* ir,
                                                 RoboHubGPIO* gpio,
                                                 rh_u32 pin,
                                                 RoboHubTimer* timer,
                                                 rh_bool active_low)
{
    if (ir == NULL || gpio == NULL || timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    ir->gpio = gpio;
    ir->pin = pin;
    ir->timer = timer;
    ir->active_low = active_low;
    ir->code = 0u;
    ir->have_code = RH_FALSE;
    ir->protocol = ROBOHUB_INFRARED_PROTOCOL_NONE;
    ir->ready = RH_FALSE;

    ir->interface.name = "ir_rx";
    ir->interface.context = ir;
    ir->interface.init = ir_op_init;
    ir->interface.deinit = ir_op_deinit;
    ir->interface.read = ir_op_read;
    ir->interface.protocol = ir_op_protocol;

    return ir_op_init(&ir->interface);
}
