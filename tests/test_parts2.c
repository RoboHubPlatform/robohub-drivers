/*
 * test_parts2.c — the second family of part drivers over fake halves.
 *
 * The checks are the mapping again: a thermistor's resistance becomes degrees, an
 * encoder's two pins become a direction, a pulse in a signal becomes a rate, and
 * an infrared burst becomes a code.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdio.h>
#include <string.h>

#include "robohub/drivers/output/gpio_switch.h"
#include "robohub/drivers/temperature/thermistor_adc.h"
#include "robohub/drivers/input/analog_adc.h"
#include "robohub/drivers/input/encoder_gpio.h"
#include "robohub/drivers/input/pulse_adc.h"
#include "robohub/drivers/input/infrared_receiver_gpio.h"

/* -------------------------------------------------------------------------- */
/*  Fakes                                                                     */
/* -------------------------------------------------------------------------- */

static int gpio_level[8];
static int gpio_writes[8];
static int gpio_write_count;

static RoboHubError gpio_init(RoboHubGPIO* s) { (void)s; return ROBOHUB_OK; }
static RoboHubError gpio_set_mode(RoboHubGPIO* s, rh_u32 p, rh_u32 m) { (void)s; (void)p; (void)m; return ROBOHUB_OK; }
static RoboHubError gpio_write(RoboHubGPIO* s, rh_u32 p, rh_u32 v)
{
    (void)s;
    if (p < 8u) { gpio_level[p] = (int)v; gpio_writes[p] = (int)v; gpio_write_count++; }
    return ROBOHUB_OK;
}
static RoboHubError gpio_read(RoboHubGPIO* s, rh_u32 p, rh_u32* v)
{
    (void)s;
    *v = (p < 8u) ? (rh_u32)gpio_level[p] : 0u;
    return ROBOHUB_OK;
}

static rh_f32 adc_level = 0.0f;
static RoboHubError adc_init(RoboHubADC* s) { (void)s; return ROBOHUB_OK; }
static RoboHubError adc_deinit(RoboHubADC* s) { (void)s; return ROBOHUB_OK; }
static rh_u8 adc_channels(RoboHubADC* s) { (void)s; return 4u; }
static rh_u8 adc_bits(RoboHubADC* s) { (void)s; return 12u; }
static RoboHubError adc_read(RoboHubADC* s, rh_u8 ch, rh_f32* level)
{
    (void)s; (void)ch;
    *level = adc_level;
    return ROBOHUB_OK;
}

static rh_millis clock_ms = 0;
static rh_micros clock_us = 0;
static rh_millis timer_ms(RoboHubTimer* s) { (void)s; return clock_ms; }
/* The microsecond clock ticks, as a real one does: a driver that times
 * something waits on it moving. */
static rh_micros timer_us(RoboHubTimer* s) { (void)s; clock_us += 100; return clock_us; }
static void timer_dms(RoboHubTimer* s, rh_u32 ms) { (void)s; clock_ms += ms; clock_us += (rh_micros)ms * 1000u; }
static void timer_dus(RoboHubTimer* s, rh_u32 us) { (void)s; clock_us += us; clock_ms += us / 1000u; }

static int near(rh_f32 a, rh_f32 b) { rh_f32 d = a - b; if (d < 0) d = -d; return d < 0.05f; }

int main(void)
{
    RoboHubGPIO gpio = { .name = "gpio0" };
    gpio.init = gpio_init; gpio.set_mode = gpio_set_mode; gpio.write = gpio_write; gpio.read = gpio_read;
    RoboHubADC adc = { .name = "adc0" };
    adc.init = adc_init; adc.deinit = adc_deinit; adc.channels = adc_channels;
    adc.resolution_bits = adc_bits; adc.read = adc_read;
    RoboHubTimer timer = { .name = "system_timer" };
    timer.get_millis = timer_ms; timer.get_micros = timer_us; timer.delay_ms = timer_dms; timer.delay_us = timer_dus;

    int ok = 1;

    /* An output: the switch writes the level the wiring asks for. */
    RoboHubGpioSwitch output;
    ok = ok && robohub_gpio_switch_init(&output, &gpio, 4u, RH_TRUE) == ROBOHUB_OK;
    ok = ok && gpio_write_count == 1 && gpio_writes[4] == 0;   /* off at bring-up */
    ok = ok && output.interface.set(&output.interface, RH_TRUE) == ROBOHUB_OK && gpio_writes[4] == 1;
    rh_bool on = RH_FALSE;
    ok = ok && output.interface.get(&output.interface, &on) == ROBOHUB_OK && on == RH_TRUE;

    /* A thermistor: half the divider is 10k, which is the part at 25 C. */
    RoboHubThermistorAdc thermistor;
    ok = ok && robohub_thermistor_adc_init(&thermistor, &adc, 0u, RH_FALSE) == ROBOHUB_OK;
    adc_level = 0.5f;
    rh_f32 celsius = 0.0f;
    ok = ok && thermistor.interface.get_celsius(&thermistor.interface, &celsius) == ROBOHUB_OK;
    ok = ok && near(celsius, 25.0f);   /* half the divider is the nominal resistance */
    /* The thermistor is the lower half of the divider, so more heat is a
     * *lower* level: the driver reports the temperature either way. */
    adc_level = 0.3f;
    ok = ok && thermistor.interface.get_celsius(&thermistor.interface, &celsius) == ROBOHUB_OK;
    ok = ok && celsius > 25.0f;

    /* An analog input: the level, inverted when the part says so. */
    RoboHubAnalogAdc analog;
    ok = ok && robohub_analog_adc_init(&analog, &adc, 1u, RH_TRUE) == ROBOHUB_OK;
    adc_level = 0.25f;
    rh_f32 level = 0.0f;
    ok = ok && analog.interface.get_level(&analog.interface, &level) == ROBOHUB_OK && near(level, 0.75f);

    /* An encoder: the two pins together say which way. */
    RoboHubEncoderGpio encoder;
    ok = ok && robohub_encoder_gpio_init(&encoder, &gpio, 5u, 6u, RH_FALSE) == ROBOHUB_OK;
    gpio_level[5] = 0; gpio_level[6] = 0;
    rh_i32 steps = 0; rh_bool clockwise = RH_FALSE;
    ok = ok && encoder.interface.read(&encoder.interface, &steps, &clockwise) == ROBOHUB_OK;
    /* From 00 to 10 is a step one way. */
    gpio_level[5] = 1; gpio_level[6] = 0;
    ok = ok && encoder.interface.read(&encoder.interface, &steps, &clockwise) == ROBOHUB_OK;
    ok = ok && (steps == 1 || steps == -1);

    /* A pulse: a crossing of the threshold is a beat, and the interval is the rate. */
    RoboHubPulseAdc pulse;
    ok = ok && robohub_pulse_adc_init(&pulse, &adc, &timer, 0u, 0.5f) == ROBOHUB_OK;
    adc_level = 0.1f;
    rh_u16 bpm = 0; rh_bool beat = RH_FALSE;
    ok = ok && pulse.interface.read(&pulse.interface, &bpm, &beat) == ROBOHUB_OK && !beat;
    clock_ms += 500;
    adc_level = 0.9f;
    ok = ok && pulse.interface.read(&pulse.interface, &bpm, &beat) == ROBOHUB_OK && beat;
    clock_ms += 500;
    adc_level = 0.1f;
    ok = ok && pulse.interface.read(&pulse.interface, &bpm, &beat) == ROBOHUB_OK;
    adc_level = 0.9f;   /* 500 ms after the last beat */
    ok = ok && pulse.interface.read(&pulse.interface, &bpm, &beat) == ROBOHUB_OK && beat;
    ok = ok && bpm == 120;   /* a beat every 500 ms is 120 a minute */

    /* An infrared receiver: a burst that is not NEC is no code. */
    RoboHubInfraredReceiverGpio receiver;
    ok = ok && robohub_infrared_receiver_gpio_init(&receiver, &gpio, 7u, &timer, RH_TRUE) == ROBOHUB_OK;
    rh_u32 code = 0; rh_bool received = RH_FALSE;
    ok = ok && receiver.interface.read(&receiver.interface, &code, &received) == ROBOHUB_OK;
    ok = ok && !received;   /* nothing arrived */

    printf(ok ? "Parts2 OK\n" : "Parts2 FAILED\n");
    return ok ? 0 : 1;
}
