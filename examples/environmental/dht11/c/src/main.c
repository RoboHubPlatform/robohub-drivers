/**
 * @file main.c
 * @brief dht11 — read the DHT11 module and print it.
 *
 * The DHT11 reports temperature and humidity together in one frame. The board
 * module's support package resolves the platform's single-wire line and timer
 * and hands them to the driver, so this file names neither: it opens the module
 * and reads the two values.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdint.h>

#include "platform.h"
#include "types.h"
#include "error.h"
#include "robohub/hal/console.h"
#include "dht11_module.h"

#define REPORT_PERIOD_MS 2000u

/* -------------------------------------------------------------------------- */
/*  Console helpers                                                           */
/* -------------------------------------------------------------------------- */

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

/* A fixed-point value with three decimals, built by hand so the console does not
 * need the C library's floating-point formatting pulled in. */
static void emit_milli(int32_t milli)
{
    emit_i32(milli / 1000);
    emit(".");
    int32_t fraction = milli % 1000;
    if (fraction < 0) {
        fraction = -fraction;
    }
    if (fraction < 100) {
        emit("0");
    }
    if (fraction < 10) {
        emit("0");
    }
    emit_i32(fraction);
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        for (;;) {
        }
    }

    robohub_dht11_module_bsp_init();

    if (robohub_dht11_module_open() != ROBOHUB_OK) {
        for (;;) {
            emit("no DHT11 module: is the platform's dht HAL up?\n");
            robohub_dht11_module_sleep_ms(2000u);
        }
    }

    emit("\nDHT11 temperature and humidity\n");
    emit("------------------------------\n");

    for (;;) {
        rh_f32 celsius = 0.0f;
        rh_f32 percent = 0.0f;

        if (robohub_dht11_module_read(&celsius, &percent) != ROBOHUB_OK) {
            emit("read failed\n");
        } else {
            emit("T = ");
            emit_milli((int32_t)(celsius * 1000.0f + (celsius >= 0.0f ? 0.5f : -0.5f)));
            emit(" C, H = ");
            emit_milli((int32_t)(percent * 1000.0f + 0.5f));
            emit(" %\n");
        }

        robohub_dht11_module_sleep_ms(REPORT_PERIOD_MS);
    }
}
