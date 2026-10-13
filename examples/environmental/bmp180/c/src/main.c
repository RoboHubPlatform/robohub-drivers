/**
 * @file main.c
 * @brief bmp180 — read the BMP180 module and print it.
 *
 * The BMP180 measures temperature and pressure and computes altitude from the
 * pressure. The board module's support package resolves the platform's I2C bus
 * and hands it to the driver, so this file names neither: it opens the module
 * and reads the three values.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdint.h>

#include "platform.h"
#include "types.h"
#include "error.h"
#include "robohub/hal/console.h"
#include "bsp.h"

#define REPORT_PERIOD_MS 1500u

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

/* A fixed-point value, built by hand so the console does not need the C
 * library's floating-point formatting pulled in. */
static void emit_fixed(int32_t value, int32_t scale)
{
    int32_t whole = value / scale;
    int32_t fraction = value % scale;
    if (fraction < 0) {
        fraction = -fraction;
    }

    emit_i32(whole);
    emit(".");
    if (scale == 1000 && fraction < 100) {
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

    robohub_bmp180_module_bsp_init();

    if (robohub_bmp180_module_open() != ROBOHUB_OK) {
        for (;;) {
            emit("no BMP180 module: is the platform's i2c HAL up?\n");
            robohub_bmp180_module_sleep_ms(2000u);
        }
    }

    emit("\nBMP180 temperature, pressure and altitude\n");
    emit("-----------------------------------------\n");

    for (;;) {
        rh_f32 celsius = 0.0f;
        rh_f32 pascals = 0.0f;
        rh_f32 metres = 0.0f;

        if (robohub_bmp180_module_read(&celsius, &pascals, &metres) != ROBOHUB_OK) {
            emit("read failed\n");
        } else {
            emit("T = ");
            emit_fixed((int32_t)(celsius * 1000.0f + (celsius >= 0.0f ? 0.5f : -0.5f)), 1000);
            emit(" C, P = ");
            emit_i32((int32_t)(pascals + 0.5f));
            emit(" Pa, alt = ");
            emit_fixed((int32_t)(metres * 100.0f + (metres >= 0.0f ? 0.5f : -0.5f)), 100);
            emit(" m\n");
        }

        robohub_bmp180_module_sleep_ms(REPORT_PERIOD_MS);
    }
}
