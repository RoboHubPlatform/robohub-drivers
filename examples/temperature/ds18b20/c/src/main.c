/**
 * @file main.c
 * @brief ds18b20 — read the DS18B20 temperature module and print it, in C.
 *
 * Written against the driver's own API: the bus and the timer are looked up in
 * the registry, the device is a value this program owns, and the reading comes
 * back as degrees Celsius. Nothing here names the board or the part's bus,
 * which is the whole point of the driver speaking through contracts.
 *
 * The program does not return. A board whose console is its own USB port is
 * enumerated by a stack running in the background, and a program that exits —
 * on a missing sensor, say — leaves the host with a failed enumeration. Every
 * failure is reported and the loop continues, which is also what makes a
 * missing sensor readable instead of silent.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdint.h>

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "error.h"
#include "robohub/hal/console.h"
#include "robohub/hal/onewire.h"
#include "robohub/hal/timer.h"
#include "ds18b20.h"

/* The names the Pico's board support package registers its bus and timer under;
 * the same names the module's own BSP defaults to. */
#define ONEWIRE_NAME "onewire0"
#define TIMER_NAME   "system_timer"

#define REPORT_PERIOD_MS 1500u

static void emit(const char* text)
{
    (void)robohub_console_puts(text);
}

static void emit_i32(int32_t value)
{
    char digits[12];
    uint32_t count = 0;
    uint32_t magnitude;

    if (value < 0) {
        emit("-");
        magnitude = (uint32_t)(-(int64_t)value);
    } else {
        magnitude = (uint32_t)value;
    }

    if (magnitude == 0u) {
        digits[count++] = '0';
    }
    while (magnitude > 0u) {
        digits[count++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    }
    while (count > 0u) {
        char c = digits[--count];
        (void)robohub_console_write(&c, 1);
    }
}

/* One line, in degrees with three decimals, built by hand so the console does
 * not need the C library's floating-point formatting pulled in. */
static void report(rh_f32 celsius)
{
    int32_t milli = (int32_t)(celsius * 1000.0f + (celsius >= 0.0f ? 0.5f : -0.5f));
    int32_t whole = milli / 1000;
    int32_t fraction = milli % 1000;
    if (fraction < 0) {
        fraction = -fraction;
    }

    emit("T = ");
    emit_i32(whole);
    emit(".");
    if (fraction < 100) {
        emit("0");
    }
    if (fraction < 10) {
        emit("0");
    }
    emit_i32(fraction);
    emit(" C\n");
}

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        for (;;) {
        }
    }

    RoboHubOneWire* bus = robohub_get_onewire(ONEWIRE_NAME);
    RoboHubTimer* timer = robohub_get_timer(TIMER_NAME);

    emit("\nDS18B20 temperature, in C\n");
    emit("-------------------------\n");

    if (bus == NULL || timer == NULL) {
        for (;;) {
            emit("no 1-Wire bus registered; is the platform's onewire HAL up?\n");
            if (timer != NULL) {
                timer->delay_ms(timer, 2000u);
            }
        }
    }

    RoboHubDS18B20 sensor;
    RoboHubDS18B20Config config = {
        .bus = bus,
        .timer = timer,
        .resolution_bits = 12,
        .alarm_high_c = 30,
        .alarm_low_c = 0,
    };

    /* A part is not always ready the instant the board boots; a few attempts
     * cost nothing and turn a race into a reading. */
    RoboHubError init_err = ROBOHUB_ERR_IO;
    for (rh_u32 attempt = 0; attempt < 5u; attempt++) {
        init_err = robohub_ds18b20_init(&sensor, &config);
        if (ROBOHUB_SUCCEEDED(init_err)) {
            break;
        }
        timer->delay_ms(timer, 200u);
    }
    if (ROBOHUB_FAILED(init_err)) {
        for (;;) {
            emit("no DS18B20 answered on the bus; check the wiring\n");
            timer->delay_ms(timer, 2000u);
        }
    }

    for (;;) {
        rh_f32 celsius = 0.0f;
        if (ROBOHUB_SUCCEEDED(robohub_ds18b20_get_celsius(&sensor, &celsius))) {
            report(celsius);
        } else {
            emit("read failed\n");
        }
        timer->delay_ms(timer, REPORT_PERIOD_MS);
    }
}
