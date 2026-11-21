/*
 * test_sht4x.c — the SHT4x driver against an I2C double (DRV-ENV-0007…0009).
 *
 * The double plays the part: it takes one-byte commands, and answers a
 * measurement or a serial-number read with six bytes and their CRCs, or fails
 * the transfer as an unplugged part does.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "sht4x.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char* what)
{
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", what);
    if (!ok) {
        failures++;
    }
}

static int near(rh_f32 a, rh_f32 b, rh_f32 tolerance)
{
    return fabsf(a - b) <= tolerance;
}

/* ---- the timer ----------------------------------------------------------- */

static rh_millis clock_ms;
static rh_millis timer_ms(RoboHubTimer* s) { (void)s; return clock_ms; }
static void timer_delay_ms(RoboHubTimer* s, rh_u32 ms) { (void)s; clock_ms += ms; }

/* ---- the part ------------------------------------------------------------ */

static rh_u8 last_command;
static rh_u8 last_address;
static rh_millis command_at;
static rh_u16 part_t = 0x6666u;     /* 25.0 °C   */
static rh_u16 part_rh = 0x6666u;    /* 44.0 %RH  */
static rh_u32 part_serial = 0x12345678u;
static int part_unplugged;
static int corrupt_word;            /* 1: the temperature's CRC, 2: the humidity's */
static int resets;
static rh_u32 init_speed;

static void put_word(rh_u8* out, rh_u16 word)
{
    out[0] = (rh_u8)(word >> 8);
    out[1] = (rh_u8)word;
    out[2] = robohub_sht4x_crc8(out, 2u);
}

static RoboHubError bus_init(RoboHubI2C* s, rh_u32 speed_hz) { (void)s; init_speed = speed_hz; return ROBOHUB_OK; }

static RoboHubError bus_write(RoboHubI2C* s, rh_u8 addr, const rh_u8* data, rh_size len)
{
    (void)s;
    if (part_unplugged || addr != 0x44u || len != 1u) {
        return ROBOHUB_ERR_IO;          /* no acknowledge */
    }
    last_address = addr;
    last_command = data[0];
    command_at = clock_ms;
    if (data[0] == ROBOHUB_SHT4X_CMD_SOFT_RESET) {
        resets++;
    }
    return ROBOHUB_OK;
}

static RoboHubError bus_read(RoboHubI2C* s, rh_u8 addr, rh_u8* data, rh_size len)
{
    (void)s;
    if (part_unplugged || addr != 0x44u || len != 6u) {
        return ROBOHUB_ERR_IO;
    }
    if (last_command == ROBOHUB_SHT4X_CMD_MEASURE_HIGH) {
        if (clock_ms - command_at < 9u) {
            return ROBOHUB_ERR_IO;      /* read before the measurement is done: NACK */
        }
        put_word(&data[0], part_t);
        put_word(&data[3], part_rh);
    } else if (last_command == ROBOHUB_SHT4X_CMD_SERIAL) {
        put_word(&data[0], (rh_u16)(part_serial >> 16));
        put_word(&data[3], (rh_u16)part_serial);
    } else {
        return ROBOHUB_ERR_IO;
    }
    if (corrupt_word == 1) {
        data[2] ^= 0x01u;
    } else if (corrupt_word == 2) {
        data[5] ^= 0x01u;
    }
    return ROBOHUB_OK;
}

int main(void)
{
    RoboHubTimer timer = { .name = "system_timer", .get_millis = timer_ms, .delay_ms = timer_delay_ms };
    RoboHubI2C bus = { .name = "i2c0", .init = bus_init, .write = bus_write, .read = bus_read };

    /* The datasheet's worked example: CRC-8(0xBEEF) is 0x92. */
    // @verifies DRV-ENV-0008
    const rh_u8 beef[2] = { 0xBEu, 0xEFu };
    check(robohub_sht4x_crc8(beef, 2u) == 0x92u, "CRC-8 of 0xBEEF is 0x92, as the datasheet gives");
    check(near(robohub_sht4x_celsius(0x0000u), -45.0f, 0.001f) && near(robohub_sht4x_celsius(0xFFFFu), 130.0f, 0.001f),
          "temperature spans -45 to 130 degrees C");
    check(robohub_sht4x_percent(0x0000u) == 0.0f && robohub_sht4x_percent(0xFFFFu) == 100.0f,
          "humidity below 0 and above 100 percent is clamped");
    check(near(robohub_sht4x_percent(0x8000u), 56.5f, 0.01f), "humidity is -6 + 125 S / 65535");

    /* Bring-up: a reset, then the serial number. */
    // @verifies DRV-ENV-0007, DRV-ENV-0009
    RoboHubSHT4x sensor;
    RoboHubSHT4xConfig config = { .bus = &bus, .timer = &timer, .address = 0u };
    check(robohub_sht4x_init(&sensor, &config) == ROBOHUB_OK, "the part comes up");
    check(resets == 1 && sensor.serial == 0x12345678u, "it is reset and its serial number read");
    check(init_speed == 100000u && sensor.address == 0x44u, "on 100 kHz, at 0x44 by default");

    /* A measurement: the command, the datasheet's wait, the two words. */
    rh_f32 celsius = 0.0f;
    rh_f32 percent = 0.0f;
    check(robohub_sht4x_read(&sensor, &celsius, &percent) == ROBOHUB_OK && last_command == 0xFDu,
          "a measurement is the high-precision command");
    check(near(celsius, 25.0f, 0.01f) && near(percent, 44.0f, 0.01f), "25.0 degrees C and 44.0 percent");
    check(sensor.temperature.get_celsius(&sensor.temperature, &celsius) == ROBOHUB_OK &&
          sensor.humidity.get_humidity(&sensor.humidity, &percent) == ROBOHUB_OK,
          "read through the temperature and humidity interfaces");

    /* A corrupted word is no value. */
    // @verifies DRV-ENV-0008
    for (int word = 1; word <= 2; word++) {
        corrupt_word = word;
        celsius = -1000.0f;
        percent = -1000.0f;
        RoboHubError err = robohub_sht4x_read(&sensor, &celsius, &percent);
        check(err == ROBOHUB_ERR_PROTOCOL && celsius == -1000.0f && percent == -1000.0f,
              word == 1 ? "a bad temperature CRC is a protocol error, and nothing is written"
                        : "a bad humidity CRC is a protocol error, and nothing is written");
    }
    corrupt_word = 0;

    /* Unplugged: an error, no value, and a reset before the next try. */
    // @verifies DRV-ENV-0009
    part_unplugged = 1;
    celsius = -1000.0f;
    check(robohub_sht4x_read(&sensor, &celsius, NULL) == ROBOHUB_ERR_IO && celsius == -1000.0f,
          "a part that does not answer is an I/O error, and nothing is written");
    check(robohub_sht4x_read(&sensor, &celsius, NULL) == ROBOHUB_ERR_IO, "and stays one while it is gone");
    part_unplugged = 0;
    int before = resets;
    part_t = 0x8000u;
    check(robohub_sht4x_read(&sensor, &celsius, NULL) == ROBOHUB_OK && resets == before + 1,
          "plugged back, it is reset and read again");
    check(near(celsius, -45.0f + 175.0f * 32768.0f / 65535.0f, 0.01f), "with its new temperature");

    /* No part at initialisation is said, not read as zero. */
    // @verifies DRV-ENV-0009
    part_unplugged = 1;
    RoboHubSHT4x missing;
    check(robohub_sht4x_init(&missing, &config) == ROBOHUB_ERR_NOT_FOUND, "no part at bring-up is not found");
    check(robohub_sht4x_read(&missing, &celsius, NULL) == ROBOHUB_ERR_STATE, "and it is not read");
    part_unplugged = 0;

    /* Arguments. */
    RoboHubSHT4xConfig no_timer = { .bus = &bus };
    check(robohub_sht4x_init(&missing, &no_timer) == ROBOHUB_ERR_INVALID_ARG, "a timer is required");
    check(robohub_sht4x_read(&sensor, NULL, NULL) == ROBOHUB_ERR_INVALID_ARG, "a read must want something");

    printf("%s: %d failure(s)\n", failures == 0 ? "sht4x ok" : "sht4x FAILED", failures);
    return failures == 0 ? 0 : 1;
}
