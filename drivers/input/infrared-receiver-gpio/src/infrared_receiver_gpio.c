/*
 * infrared_receiver_gpio.c — an infrared receiver on a GPIO.
 *
 * What reaches the pin is a burst of pulses, and a protocol is what gives it a
 * meaning. NEC is decoded here: a header mark of about nine milliseconds, a
 * space of about four and a half, then thirty-two bits sent least-significant
 * first — each a mark of about 560 microseconds followed by a space of about
 * 560 for a zero or about 1690 for a one (a bit period of 1.125 or 2.25 ms) —
 * and a closing mark.
 *
 * The thirty-two bits are four bytes: an address, its inverse, a command and
 * its inverse. That is the original NEC; most remotes now send the extended
 * form, whose second byte is the high byte of a 16-bit address rather than the
 * inverse of the first. The command's inverse is in both, and it is what checks
 * a code: a frame whose command does not invert is refused. The address is read
 * as 8 bits when its second byte inverts it and as 16 bits when it does not, and
 * the code is `(address << 8) | command`.
 *
 * A held button sends a repeat code — the header mark, a 2.25 ms space and a
 * mark — which carries no code and is reported as nothing received.
 *
 * The pulses are timed by polling the pin. The shortest thing that has to be
 * told apart is a zero's space against a one's, about a millisecond apart, so a
 * poll of tens of microseconds is enough.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "infrared_receiver_gpio.h"

#include <stddef.h>

/* The NEC timings, in microseconds: how long the line holds each level. The
 * window each one is allowed to fall in is what tells a zero from a one. */
#define NEC_HEADER_MARK_US      9000u
#define NEC_HEADER_SPACE_US     4500u
#define NEC_BIT_MARK_US          560u
#define NEC_ZERO_SPACE_US        563u
#define NEC_ONE_SPACE_US        1688u
/* How far a timing may be from the one it is compared with. */
#define NEC_TOLERANCE_US         400u
/* A level held longer than this ends the burst: the silence after a frame, or a
 * line stuck at one level. The longest level inside a frame is the header mark. */
#define NEC_END_US             10000u
/* How long to wait for the first mark before deciding nothing is arriving. */
#define NEC_IDLE_TIMEOUT_US    20000u

/* How many levels a burst may have. A NEC frame is a header mark and space,
 * thirty-two marks and spaces and a closing mark: sixty-seven. */
#define NEC_MAX_EDGES           72u
#define NEC_FRAME_LEVELS        (2u + 2u * 32u + 1u)

/* Is the line at mark? The module drives it away from idle for a mark; which
 * level that is, is the wiring (`active_low`). A pin that cannot be read is
 * idle. */
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

/* Time one burst: wait for its first mark, then record how long each level is
 * held — mark, space, mark, ... — until a level outlasts NEC_END_US. Returns
 * how many durations were recorded; zero when nothing arrived. */
static rh_size ir_capture(RoboHubInfraredReceiverGpio* ir, rh_u32* levels, rh_size max)
{
    rh_micros started = ir->timer->get_micros(ir->timer);
    rh_micros now = started;
    while (ir_line(ir) != RH_TRUE) {
        now = ir->timer->get_micros(ir->timer);
        if ((now - started) > NEC_IDLE_TIMEOUT_US) {
            return 0u;   /* nothing is arriving */
        }
    }

    rh_size count = 0u;
    rh_bool mark = RH_TRUE;
    rh_micros last_edge = now;
    while (count < max) {
        now = ir->timer->get_micros(ir->timer);
        rh_bool line = ir_line(ir);
        if (line != mark) {
            levels[count++] = (rh_u32)(now - last_edge);
            last_edge = now;
            mark = line;
        } else if ((now - last_edge) > NEC_END_US) {
            break;       /* the burst is over, or the line is stuck */
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

/* Decode a captured burst as NEC, standard or extended, into the code. */
// @req DRV-IN-0005, DRV-IN-0006
static rh_bool ir_decode(const rh_u32* levels, rh_size count, rh_u32* code)
{
    if (count < NEC_FRAME_LEVELS) {
        return RH_FALSE;   /* a repeat code, a frame cut short, or noise */
    }
    if (!near(levels[0], NEC_HEADER_MARK_US) || !near(levels[1], NEC_HEADER_SPACE_US)) {
        return RH_FALSE;
    }
    rh_u32 frame = 0u;
    for (rh_u32 bit = 0; bit < 32u; bit++) {
        rh_u32 mark = levels[2u + 2u * bit];
        rh_u32 space = levels[3u + 2u * bit];
        if (!near(mark, NEC_BIT_MARK_US)) {
            return RH_FALSE;
        }
        if (near(space, NEC_ONE_SPACE_US)) {
            frame |= 1u << bit;
        } else if (!near(space, NEC_ZERO_SPACE_US)) {
            return RH_FALSE;
        }
    }
    if (!near(levels[2u + 2u * 32u], NEC_BIT_MARK_US)) {
        return RH_FALSE;   /* the closing mark */
    }

    rh_u32 address_low = frame & 0xFFu;
    rh_u32 address_high = (frame >> 8) & 0xFFu;
    rh_u32 command = (frame >> 16) & 0xFFu;
    rh_u32 command_inverse = (frame >> 24) & 0xFFu;
    /* The command is checked by its inverse in both forms; a frame that fails
     * it is noise or a remote misheard. */
    if ((command ^ command_inverse) != 0xFFu) {
        return RH_FALSE;
    }
    /* The address: 8 bits when the second byte is its inverse (standard NEC),
     * else 16 bits, low byte first (extended NEC). */
    rh_u32 address = ((address_low ^ address_high) == 0xFFu)
                         ? address_low
                         : (address_low | (address_high << 8));
    *code = (address << 8) | command;
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

    rh_u32 levels[NEC_MAX_EDGES];
    rh_size count = ir_capture(ir, levels, NEC_MAX_EDGES);
    ir->have_code = RH_FALSE;
    ir->protocol = ROBOHUB_INFRARED_PROTOCOL_NONE;
    *code = 0u;

    rh_u32 decoded = 0u;
    if (ir_decode(levels, count, &decoded)) {
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
