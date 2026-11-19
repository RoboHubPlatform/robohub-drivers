/*
 * test_ir_receiver.c — the infrared receiver over a fake pin that replays a
 * remote's burst.
 *
 * The fake timer moves 100 µs each time it is read, and the fake pin answers
 * with the level a TSOP-style receiver (active low) would show at that moment of
 * a NEC transmission: a 9 ms header mark, a 4.5 ms space, thirty-two bits
 * least-significant first, each a 560 µs mark and a 563 µs (zero) or 1688 µs
 * (one) space, and a closing 560 µs mark. The frames are the ones real remotes
 * send:
 *
 *   standard  address, ~address, command, ~command   (the original NEC)
 *   extended  address low, address high, command, ~command — a 16-bit address
 *             with no complement, which most current remotes use
 *
 * and the checks are that both decode to (address << 8) | command, that a
 * command whose inverse does not check is refused, that a repeat code is no new
 * code, and that silence and a stuck line are nothing received.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdio.h>
#include <string.h>

#include "robohub/drivers/input/infrared_receiver_gpio.h"

static int failures;

#define CHECK(cond, what) do { \
    if (cond) { printf("[PASS] %s\n", what); } \
    else { printf("[FAIL] %s (%s:%d)\n", what, __FILE__, __LINE__); failures++; } \
} while (0)

/* -------------------------------------------------------------------------- */
/*  A clock, and a pin that replays a burst against it                        */
/* -------------------------------------------------------------------------- */

static rh_micros clock_us;
static rh_micros timer_us(RoboHubTimer* s) { (void)s; clock_us += 100u; return clock_us; }
static rh_millis timer_ms(RoboHubTimer* s) { (void)s; return (rh_millis)(clock_us / 1000u); }

/* The burst: durations alternating mark, space, mark, ... from `wave_start`. */
static rh_u32 wave[80];
static rh_size wave_len;
static rh_micros wave_start;
static int stuck_mark;

static int mark_at(rh_micros t)
{
    if (stuck_mark) {
        return 1;
    }
    if (t < wave_start) {
        return 0;
    }
    rh_micros edge = wave_start;
    for (rh_size i = 0; i < wave_len; i++) {
        edge += wave[i];
        if (t < edge) {
            return (i % 2u) == 0u;   /* even entries are marks */
        }
    }
    return 0;
}

static RoboHubError gpio_init(RoboHubGPIO* s) { (void)s; return ROBOHUB_OK; }
static RoboHubError gpio_set_mode(RoboHubGPIO* s, rh_u32 p, rh_u32 m) { (void)s; (void)p; (void)m; return ROBOHUB_OK; }
static RoboHubError gpio_read(RoboHubGPIO* s, rh_u32 p, rh_u32* v)
{
    (void)s; (void)p;
    *v = mark_at(clock_us) ? 0u : 1u;   /* active low: a mark pulls the line down */
    return ROBOHUB_OK;
}

static void push(rh_u32 us) { wave[wave_len++] = us; }

/* Arm the pin with a NEC frame of four bytes, as sent, starting shortly. */
static void nec_frame(rh_u8 b0, rh_u8 b1, rh_u8 b2, rh_u8 b3)
{
    rh_u8 bytes[4] = { b0, b1, b2, b3 };
    stuck_mark = 0;
    wave_len = 0;
    push(9000u);
    push(4500u);
    for (int i = 0; i < 4; i++) {
        for (int bit = 0; bit < 8; bit++) {
            push(560u);
            push(((bytes[i] >> bit) & 1u) ? 1688u : 563u);
        }
    }
    push(560u);                          /* the closing mark */
    wave_start = clock_us + 700u;
}

/* The repeat code a remote sends while a button is held. */
static void nec_repeat(void)
{
    stuck_mark = 0;
    wave_len = 0;
    push(9000u);
    push(2250u);
    push(560u);
    wave_start = clock_us + 700u;
}

static void silence(void)
{
    stuck_mark = 0;
    wave_len = 0;
    wave_start = clock_us;
}

static int read_code(RoboHubInfraredReceiverGpio* rx, rh_u32* code)
{
    rh_bool received = RH_FALSE;
    *code = 0xDEADBEEFu;
    if (rx->interface.read(&rx->interface, code, &received) != ROBOHUB_OK) {
        return -1;
    }
    return received ? 1 : 0;
}

// @verifies DRV-IN-0005, DRV-IN-0006
int main(void)
{
    RoboHubGPIO gpio = { .name = "gpio0" };
    gpio.init = gpio_init; gpio.set_mode = gpio_set_mode; gpio.read = gpio_read;
    RoboHubTimer timer = { .name = "system_timer" };
    timer.get_micros = timer_us; timer.get_millis = timer_ms;

    RoboHubInfraredReceiverGpio rx;
    CHECK(robohub_infrared_receiver_gpio_init(&rx, &gpio, 7u, &timer, RH_TRUE) == ROBOHUB_OK,
          "the receiver binds to a pin and a timer");

    rh_u32 code = 0;

    /* Standard NEC: address 0x00 with its complement, command 0x45 (the
     * "power" key of the common 21-key remotes). */
    nec_frame(0x00u, 0xFFu, 0x45u, 0xBAu);
    CHECK(read_code(&rx, &code) == 1, "a standard NEC frame is received");
    CHECK(code == 0x0045u, "a standard frame is (8-bit address << 8) | command");
    CHECK(rx.interface.protocol(&rx.interface) == ROBOHUB_INFRARED_PROTOCOL_NEC,
          "the protocol is NEC");

    nec_frame(0x04u, 0xFBu, 0x08u, 0xF7u);
    CHECK(read_code(&rx, &code) == 1 && code == 0x0408u,
          "a standard frame with address 0x04 and command 0x08");

    /* Extended NEC: a 16-bit address, 0xBE40 sent low byte first, no
     * complement; the command still carries its inverse. */
    nec_frame(0x40u, 0xBEu, 0x12u, 0xEDu);
    CHECK(read_code(&rx, &code) == 1, "an extended NEC frame is received");
    CHECK(code == 0xBE4012u, "an extended frame is (16-bit address << 8) | command");

    nec_frame(0x86u, 0x6Bu, 0x1Cu, 0xE3u);
    CHECK(read_code(&rx, &code) == 1 && code == 0x6B861Cu,
          "an extended frame with address 0x6B86 and command 0x1C");

    /* Corrupted: the command's inverse does not check, in either format. */
    nec_frame(0x00u, 0xFFu, 0x45u, 0xBBu);
    CHECK(read_code(&rx, &code) == 0 && code == 0u,
          "a standard frame whose command inverse is wrong is refused, code zero");
    nec_frame(0x40u, 0xBEu, 0x12u, 0x12u);
    CHECK(read_code(&rx, &code) == 0, "an extended frame whose command inverse is wrong is refused");
    CHECK(rx.interface.protocol(&rx.interface) == ROBOHUB_INFRARED_PROTOCOL_NONE,
          "a refused frame reports no protocol");

    /* A frame cut short is not a code. */
    nec_frame(0x00u, 0xFFu, 0x45u, 0xBAu);
    wave_len = 2u + 2u * 20u;
    CHECK(read_code(&rx, &code) == 0, "a frame cut short is refused");

    /* A repeat code is not a new code, as before. */
    nec_repeat();
    CHECK(read_code(&rx, &code) == 0 && code == 0u, "a repeat code is no new code");

    /* Silence, and a line held at mark, are nothing received. */
    silence();
    CHECK(read_code(&rx, &code) == 0, "silence is nothing received");
    stuck_mark = 1;
    CHECK(read_code(&rx, &code) == 0, "a line stuck at mark is nothing received");

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all infrared receiver checks passed\n");
    return 0;
}
