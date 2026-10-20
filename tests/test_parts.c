/*
 * test_parts.c — the GPIO/PWM/ADC part drivers over fake HALs.
 *
 * The checks are the mapping: a contact reads closed or open through the
 * polarity it was wired with, a colour writes each channel as its fraction, a
 * tone is a frequency and a half duty, a photoresistor inverts when it is the
 * upper half of the divider, and an IR sequence gates the carrier at each mark
 * and space.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdio.h>
#include <string.h>

#include "robohub/drivers/input/button_gpio.h"
#include "robohub/drivers/output/tone_pwm.h"
#include "robohub/drivers/output/rgb_pwm.h"
#include "robohub/drivers/output/infrared_pwm.h"
#include "robohub/drivers/light/photoresistor_adc.h"

/* -------------------------------------------------------------------------- */
/*  Fakes                                                                     */
/* -------------------------------------------------------------------------- */

static int gpio_level;
static RoboHubError gpio_init(RoboHubGPIO* s) { (void)s; return ROBOHUB_OK; }
static RoboHubError gpio_set_mode(RoboHubGPIO* s, rh_u32 p, rh_u32 m) { (void)s; (void)p; (void)m; return ROBOHUB_OK; }
static RoboHubError gpio_write(RoboHubGPIO* s, rh_u32 p, rh_u32 v) { (void)s; (void)p; gpio_level = (int)v; return ROBOHUB_OK; }
static RoboHubError gpio_read(RoboHubGPIO* s, rh_u32 p, rh_u32* v) { (void)s; (void)p; *v = (rh_u32)gpio_level; return ROBOHUB_OK; }

static rh_u32 pwm_freq;
static rh_f32 pwm_duty[4];
static int pwm_stopped[4];
static RoboHubError pwm_init(RoboHubPWM* s) { (void)s; return ROBOHUB_OK; }
static RoboHubError pwm_deinit(RoboHubPWM* s) { (void)s; return ROBOHUB_OK; }
static rh_u8 pwm_channels(RoboHubPWM* s) { (void)s; return 4u; }
static RoboHubError pwm_set_freq(RoboHubPWM* s, rh_u32 hz) { (void)s; pwm_freq = hz; return ROBOHUB_OK; }
static RoboHubError pwm_set_duty(RoboHubPWM* s, rh_u8 ch, rh_f32 duty) { (void)s; if (ch < 4) { pwm_duty[ch] = duty; pwm_stopped[ch] = 0; } return ROBOHUB_OK; }
static RoboHubError pwm_stop(RoboHubPWM* s, rh_u8 ch) { (void)s; if (ch < 4) { pwm_stopped[ch] = 1; } return ROBOHUB_OK; }

static rh_f32 adc_level;
static RoboHubError adc_init(RoboHubADC* s) { (void)s; return ROBOHUB_OK; }
static RoboHubError adc_deinit(RoboHubADC* s) { (void)s; return ROBOHUB_OK; }
static rh_u8 adc_channels(RoboHubADC* s) { (void)s; return 4u; }
static rh_u8 adc_bits(RoboHubADC* s) { (void)s; return 12u; }
static RoboHubError adc_read(RoboHubADC* s, rh_u8 ch, rh_f32* level) { (void)s; (void)ch; *level = adc_level; return ROBOHUB_OK; }

static rh_u32 timer_last_us;
static rh_millis timer_ms(RoboHubTimer* s) { (void)s; return 0; }
static rh_micros timer_us(RoboHubTimer* s) { (void)s; return 0; }
static void timer_dms(RoboHubTimer* s, rh_u32 ms) { (void)s; (void)ms; }
static void timer_dus(RoboHubTimer* s, rh_u32 us) { timer_last_us = us; }

static int near(rh_f32 a, rh_f32 b) { rh_f32 d = a - b; if (d < 0) d = -d; return d < 0.0005f; }

int main(void)
{
    RoboHubGPIO gpio = { .name = "gpio0" };
    gpio.init = gpio_init; gpio.set_mode = gpio_set_mode; gpio.write = gpio_write; gpio.read = gpio_read;
    RoboHubPWM pwm = { .name = "pwm0" };
    pwm.init = pwm_init; pwm.deinit = pwm_deinit; pwm.channels = pwm_channels;
    pwm.set_frequency = pwm_set_freq; pwm.set_duty = pwm_set_duty; pwm.stop = pwm_stop;
    RoboHubADC adc = { .name = "adc0" };
    adc.init = adc_init; adc.deinit = adc_deinit; adc.channels = adc_channels;
    adc.resolution_bits = adc_bits; adc.read = adc_read;
    RoboHubTimer timer = { .name = "system_timer" };
    timer.get_millis = timer_ms; timer.get_micros = timer_us; timer.delay_ms = timer_dms; timer.delay_us = timer_dus;

    int ok = 1;
    rh_bool pressed = RH_FALSE;

    /* A contact wired active-low: low is closed. */
    RoboHubButtonGpio button;
    ok = ok && robohub_button_gpio_init(&button, &gpio, 3u, RH_TRUE) == ROBOHUB_OK;
    gpio_level = 0;
    ok = ok && button.interface.get(&button.interface, &pressed) == ROBOHUB_OK && pressed == RH_TRUE;
    gpio_level = 1;
    ok = ok && button.interface.get(&button.interface, &pressed) == ROBOHUB_OK && pressed == RH_FALSE;

    /* A tone is a frequency and a half duty. */
    RoboHubTonePwm tone;
    ok = ok && robohub_tone_pwm_init(&tone, &pwm, 0u) == ROBOHUB_OK;
    ok = ok && tone.interface.play(&tone.interface, 440u) == ROBOHUB_OK;
    ok = ok && pwm_freq == 440u && near(pwm_duty[0], 0.5f);
    ok = ok && tone.interface.stop(&tone.interface) == ROBOHUB_OK && pwm_stopped[0] == 1;

    /* A two-colour LED has two channels; the third byte is ignored. */
    RoboHubRgbPwm rgb;
    ok = ok && robohub_rgb_pwm_init(&rgb, &pwm, 1u, 2u, ROBOHUB_RGB_NO_CHANNEL) == ROBOHUB_OK;
    ok = ok && rgb.interface.channels(&rgb.interface) == 2u;
    ok = ok && rgb.interface.set(&rgb.interface, 255u, 0u, 0u) == ROBOHUB_OK;
    ok = ok && near(pwm_duty[1], 1.0f) && near(pwm_duty[2], 0.0f);

    /* A photoresistor as the upper half of the divider: more light, less level. */
    RoboHubPhotoresistorAdc light;
    ok = ok && robohub_photoresistor_adc_init(&light, &adc, 0u, RH_TRUE) == ROBOHUB_OK;
    adc_level = 0.25f;
    rh_f32 level = 0.0f;
    ok = ok && light.interface.get_level(&light.interface, &level) == ROBOHUB_OK && near(level, 0.75f);

    /* An IR sequence gates the carrier at each mark and space. */
    RoboHubInfraredPwm ir;
    ok = ok && robohub_infrared_pwm_init(&ir, &pwm, &timer, 3u) == ROBOHUB_OK;
    ok = ok && ir.interface.carrier_hz(&ir.interface) == ROBOHUB_INFRARED_CARRIER_HZ;
    const rh_u32 seq[3] = { 900u, 450u, 560u };
    ok = ok && ir.interface.send(&ir.interface, seq, 3u) == ROBOHUB_OK;
    ok = ok && pwm_freq == ROBOHUB_INFRARED_CARRIER_HZ && timer_last_us == 560u && pwm_stopped[3] == 1;

    printf(ok ? "Parts OK\n" : "Parts FAILED\n");
    return ok ? 0 : 1;
}
