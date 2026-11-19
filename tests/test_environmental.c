/*
 * test_environmental.c — the BMP180, the DHT11 and the DS18B20 over fake buses.
 *
 * Each part is spoken to through the HAL contract it is written against — a
 * register bus, a DHT frame line, a 1-Wire bus — and each fake plays the part:
 *
 *   BMP180   a register map holding the datasheet's example calibration, which
 *            answers a conversion command with the datasheet's example UT and
 *            UP; and the ways a part is absent: a bus that is not acknowledged,
 *            an SPI line that reads all ones or all zeros.
 *   DHT11    the frames a part sends: a reading, a bad checksum, the all-zero
 *            frame of a line nothing drives, and a bus that heard no answer.
 *   DS18B20  a scratchpad with the 1-Wire CRC-8 over it: the datasheet's
 *            temperatures, a scratchpad whose CRC does not check, resolution
 *            masking, and a bus with nothing on it.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdio.h>
#include <string.h>

#include "robohub/drivers/environmental/bmp180.h"
#include "robohub/drivers/environmental/dht11.h"
#include "robohub/drivers/temperature/ds18b20.h"

static int failures;

#define CHECK(cond, what) do { \
    if (cond) { printf("[PASS] %s\n", what); } \
    else { printf("[FAIL] %s (%s:%d)\n", what, __FILE__, __LINE__); failures++; } \
} while (0)

static int near(rh_f32 a, rh_f32 b, rh_f32 tolerance)
{
    rh_f32 d = a - b;
    if (d < 0) d = -d;
    return d <= tolerance;
}

/* -------------------------------------------------------------------------- */
/*  A timer                                                                   */
/* -------------------------------------------------------------------------- */

static rh_millis clock_ms;
static rh_u32 delays;
static rh_millis timer_ms(RoboHubTimer* s) { (void)s; return clock_ms; }
static void timer_delay_ms(RoboHubTimer* s, rh_u32 ms) { (void)s; clock_ms += ms; delays++; }

/* -------------------------------------------------------------------------- */
/*  BMP180: a register map                                                    */
/* -------------------------------------------------------------------------- */

static rh_u8 bmp_regs[256];
static RoboHubError bmp_read_error;     /* what every read returns, when set */
static rh_u8 bmp_oss;

/* The datasheet's worked example (BMP180 data sheet, "Calculating pressure and
 * temperature"): these coefficients, UT = 27898 and UP = 23843 at oss 0 give
 * 15.0 C and 69964 Pa. */
static const rh_i16 bmp_calib[11] = {
    408, -72, -14383, (rh_i16)32741, (rh_i16)32757, 23153, 6190, 4, (rh_i16)-32768, -8711, 2868
};

static void bmp_reset_part(rh_u8 chip_id)
{
    memset(bmp_regs, 0, sizeof(bmp_regs));
    bmp_regs[0xD0] = chip_id;
    for (int i = 0; i < 11; i++) {
        rh_u16 v = (rh_u16)bmp_calib[i];
        bmp_regs[0xAA + 2 * i] = (rh_u8)(v >> 8);
        bmp_regs[0xAB + 2 * i] = (rh_u8)(v & 0xFFu);
    }
    bmp_read_error = ROBOHUB_OK;
}

static RoboHubError bmp_read_reg(RoboHubBus* s, rh_u8 reg, rh_u8* data, rh_size len)
{
    (void)s;
    if (bmp_read_error != ROBOHUB_OK) {
        return bmp_read_error;
    }
    for (rh_size i = 0; i < len; i++) {
        data[i] = bmp_regs[(rh_u8)(reg + i)];
    }
    return ROBOHUB_OK;
}

static RoboHubError bmp_write_reg(RoboHubBus* s, rh_u8 reg, const rh_u8* data, rh_size len)
{
    (void)s;
    if (reg == 0xF4u && len == 1u) {
        if (data[0] == 0x2Eu) {                 /* temperature: UT */
            bmp_regs[0xF6] = (rh_u8)(27898u >> 8);
            bmp_regs[0xF7] = (rh_u8)(27898u & 0xFFu);
        } else if ((data[0] & 0x3Fu) == 0x34u) { /* pressure: UP << (8 - oss) */
            bmp_oss = (rh_u8)(data[0] >> 6);
            rh_u32 raw = 23843u << (8u - bmp_oss);
            bmp_regs[0xF6] = (rh_u8)(raw >> 16);
            bmp_regs[0xF7] = (rh_u8)(raw >> 8);
            bmp_regs[0xF8] = (rh_u8)raw;
        }
        bmp_regs[0xF4] = (rh_u8)(data[0] & ~0x20u);  /* end of conversion */
    }
    return ROBOHUB_OK;
}

// @verifies DRV-ENV-0001, DRV-ENV-0006
static void test_bmp180(RoboHubTimer* timer)
{
    RoboHubBus bus;
    memset(&bus, 0, sizeof(bus));
    bus.name = "i2c0";
    bus.read_reg = bmp_read_reg;
    bus.write_reg = bmp_write_reg;

    RoboHubBMP180 dev;
    RoboHubBMP180Config config;
    memset(&config, 0, sizeof(config));
    config.bus = &bus;
    config.timer = timer;
    config.oversampling = 0u;

    bmp_reset_part(ROBOHUB_BMP180_CHIP_ID);
    CHECK(robohub_bmp180_init(&dev, &config) == ROBOHUB_OK, "bmp180: a part that answers comes up");
    rh_f32 celsius = 0, pascals = 0, metres = 0;
    CHECK(robohub_bmp180_read(&dev, &celsius, &pascals, &metres) == ROBOHUB_OK,
          "bmp180: a reading");
    CHECK(near(celsius, 15.0f, 0.01f), "bmp180: the datasheet's UT is 15.0 C");
    CHECK(near(pascals, 69964.0f, 0.5f), "bmp180: the datasheet's UP is 69964 Pa");
    CHECK(metres > 2900.0f && metres < 3100.0f,
          "bmp180: 69964 Pa is about 3 km above the standard sea level");

    /* Absent: the bus is not acknowledged (an I2C NACK, or a timeout). */
    bmp_reset_part(ROBOHUB_BMP180_CHIP_ID);
    bmp_read_error = ROBOHUB_ERR_IO;
    CHECK(robohub_bmp180_init(&dev, &config) == ROBOHUB_ERR_NOT_FOUND,
          "bmp180: a bus error on the identity read is not found");
    bmp_read_error = ROBOHUB_ERR_TIMEOUT;
    CHECK(robohub_bmp180_init(&dev, &config) == ROBOHUB_ERR_NOT_FOUND,
          "bmp180: a bus timeout on the identity read is not found");

    /* Absent on SPI: nothing drives MISO, so the identity reads all ones or
     * all zeros. */
    bmp_reset_part(0xFFu);
    CHECK(robohub_bmp180_init(&dev, &config) == ROBOHUB_ERR_NOT_FOUND,
          "bmp180: an identity of 0xFF is not found");
    bmp_reset_part(0x00u);
    CHECK(robohub_bmp180_init(&dev, &config) == ROBOHUB_ERR_NOT_FOUND,
          "bmp180: an identity of 0x00 is not found");

    /* Present, but another part: a BMP280 answers 0x58. */
    bmp_reset_part(0x58u);
    CHECK(robohub_bmp180_init(&dev, &config) == ROBOHUB_ERR_PROTOCOL,
          "bmp180: a part with another identity is a protocol error");

    /* A bus that cannot do the transfer says so; that is not an absent part. */
    bmp_reset_part(ROBOHUB_BMP180_CHIP_ID);
    bmp_read_error = ROBOHUB_ERR_NOT_SUPPORTED;
    CHECK(robohub_bmp180_init(&dev, &config) == ROBOHUB_ERR_NOT_SUPPORTED,
          "bmp180: a bus that cannot read passes its own error");
}

/* -------------------------------------------------------------------------- */
/*  DHT11: a frame line                                                       */
/* -------------------------------------------------------------------------- */

static rh_u8 dht_frame[5];
static RoboHubError dht_error;

static RoboHubError dht_read(RoboHubDHT* s, rh_u8* data, rh_size len)
{
    (void)s;
    if (dht_error != ROBOHUB_OK) {
        return dht_error;
    }
    memcpy(data, dht_frame, len < 5u ? len : 5u);
    return ROBOHUB_OK;
}

static void dht_set(rh_u8 h, rh_u8 hd, rh_u8 t, rh_u8 td, rh_u8 sum)
{
    dht_frame[0] = h; dht_frame[1] = hd; dht_frame[2] = t; dht_frame[3] = td; dht_frame[4] = sum;
    dht_error = ROBOHUB_OK;
}

// @verifies DRV-ENV-0004, DRV-ENV-0006
static void test_dht11(RoboHubTimer* timer)
{
    RoboHubDHT bus;
    memset(&bus, 0, sizeof(bus));
    bus.name = "dht0";
    bus.read = dht_read;

    RoboHubDHT11 dev;
    RoboHubDHT11Config config;
    memset(&config, 0, sizeof(config));
    config.bus = &bus;
    config.timer = timer;
    CHECK(robohub_dht11_init(&dev, &config) == ROBOHUB_OK, "dht11: init");

    rh_f32 celsius = 0, percent = 0;
    dht_set(55u, 0u, 24u, 3u, (rh_u8)(55u + 24u + 3u));
    CHECK(robohub_dht11_read(&dev, &celsius, &percent) == ROBOHUB_OK &&
          near(celsius, 24.3f, 0.01f) && near(percent, 55.0f, 0.01f),
          "dht11: a frame is 24.3 C and 55 %");

    rh_millis before = clock_ms;
    dht_set(56u, 0u, 24u, 0u, (rh_u8)(56u + 24u));
    CHECK(robohub_dht11_read(&dev, NULL, &percent) == ROBOHUB_OK && near(percent, 56.0f, 0.01f) &&
          clock_ms - before >= ROBOHUB_DHT11_MIN_INTERVAL_MS,
          "dht11: a second read is paced by the part's interval");

    /* The checksum is the low byte of the sum: 200 + 100 wraps. */
    dht_set(90u, 110u, 50u, 50u, (rh_u8)(90u + 110u + 50u + 50u));
    CHECK(robohub_dht11_read(&dev, &celsius, NULL) == ROBOHUB_OK,
          "dht11: the checksum is the sum's low byte");

    dht_set(55u, 0u, 24u, 3u, 0x00u);
    CHECK(robohub_dht11_read(&dev, &celsius, &percent) == ROBOHUB_ERR_PROTOCOL,
          "dht11: a checksum that does not match is a protocol error");

    /* The frame of a line nothing drives: all zeros, whose checksum (zero)
     * checks. No DHT11 reports 0 % humidity; this is no part. */
    dht_set(0u, 0u, 0u, 0u, 0u);
    CHECK(robohub_dht11_read(&dev, &celsius, &percent) == ROBOHUB_ERR_NOT_FOUND,
          "dht11: an all-zero frame is not found");

    /* A bus that heard no answer to the start signal says so. */
    dht_error = ROBOHUB_ERR_NOT_FOUND;
    CHECK(robohub_dht11_read(&dev, &celsius, &percent) == ROBOHUB_ERR_NOT_FOUND,
          "dht11: no answer on the bus is not found");
    dht_error = ROBOHUB_ERR_TIMEOUT;
    CHECK(robohub_dht11_read(&dev, &celsius, &percent) == ROBOHUB_ERR_NOT_FOUND,
          "dht11: a bus timeout waiting for the answer is not found");
}

/* -------------------------------------------------------------------------- */
/*  DS18B20: a 1-Wire bus                                                     */
/* -------------------------------------------------------------------------- */

static rh_bool ow_present;
static rh_u8 ow_scratchpad[9];
static rh_size ow_read_index;
static rh_u8 ow_last_command;
static rh_size ow_written;
static rh_u32 ow_copies;

/* The 1-Wire CRC-8, written here to build scratchpads; it is checked against
 * a known vector below, so the driver is not tested against itself. */
static rh_u8 crc8(const rh_u8* data, rh_size len)
{
    rh_u8 crc = 0u;
    for (rh_size i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 1u) ? (rh_u8)((crc >> 1) ^ 0x8Cu) : (rh_u8)(crc >> 1);
        }
    }
    return crc;
}

static void ow_set_scratchpad(rh_u16 raw, rh_u8 config, rh_bool good_crc)
{
    ow_scratchpad[0] = (rh_u8)(raw & 0xFFu);
    ow_scratchpad[1] = (rh_u8)(raw >> 8);
    ow_scratchpad[2] = 30u;     /* TH: the driver's default alarm high */
    ow_scratchpad[3] = 0u;      /* TL */
    ow_scratchpad[4] = config;
    ow_scratchpad[5] = 0xFFu;
    ow_scratchpad[6] = 0x0Cu;
    ow_scratchpad[7] = 0x10u;
    ow_scratchpad[8] = (rh_u8)(crc8(ow_scratchpad, 8u) ^ (good_crc ? 0u : 0x01u));
}

static RoboHubError ow_reset(RoboHubOneWire* s, rh_bool* present)
{
    (void)s;
    *present = ow_present;
    ow_read_index = 0u;
    ow_written = 0u;
    return ROBOHUB_OK;
}

static RoboHubError ow_write_byte(RoboHubOneWire* s, rh_u8 byte)
{
    (void)s;
    if (ow_written == 1u) {
        ow_last_command = byte;          /* after SKIP ROM */
        if (byte == 0x48u) {
            ow_copies++;
        }
    }
    ow_written++;
    return ROBOHUB_OK;
}

static RoboHubError ow_read_byte(RoboHubOneWire* s, rh_u8* byte)
{
    (void)s;
    *byte = (ow_last_command == 0xBEu && ow_read_index < 9u) ? ow_scratchpad[ow_read_index++] : 0xFFu;
    return ROBOHUB_OK;
}

// @verifies DRV-TEMP-0001, DRV-TEMP-0002, DRV-TEMP-0003
static void test_ds18b20(RoboHubTimer* timer)
{
    /* The power-on scratchpad every DS18B20 reads back: 85 C, TH 75, TL 70,
     * 12 bits, CRC 0x1C. */
    const rh_u8 power_on[9] = { 0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C };
    CHECK(crc8(power_on, 8u) == power_on[8], "ds18b20: the test's CRC-8 matches a real scratchpad");

    RoboHubOneWire bus;
    memset(&bus, 0, sizeof(bus));
    bus.name = "onewire0";
    bus.reset = ow_reset;
    bus.write_byte = ow_write_byte;
    bus.read_byte = ow_read_byte;

    RoboHubDS18B20 dev;
    RoboHubDS18B20Config config;
    memset(&config, 0, sizeof(config));
    config.bus = &bus;
    config.timer = timer;

    ow_present = RH_TRUE;
    ow_copies = 0u;
    ow_set_scratchpad(0x0191u, 0x7Fu, RH_TRUE);   /* +25.0625 C, 12 bits */
    CHECK(robohub_ds18b20_init(&dev, &config) == ROBOHUB_OK, "ds18b20: init");
    CHECK(ow_copies == 0u, "ds18b20: a part already set up is not written to EEPROM");

    rh_f32 celsius = 0;
    CHECK(robohub_ds18b20_get_celsius(&dev, &celsius) == ROBOHUB_OK && near(celsius, 25.0625f, 0.0001f),
          "ds18b20: 0x0191 is +25.0625 C");
    ow_set_scratchpad(0xFF5Eu, 0x7Fu, RH_TRUE);
    CHECK(robohub_ds18b20_get_celsius(&dev, &celsius) == ROBOHUB_OK && near(celsius, -10.125f, 0.0001f),
          "ds18b20: 0xFF5E is -10.125 C");
    ow_set_scratchpad(0xFC90u, 0x7Fu, RH_TRUE);
    CHECK(robohub_ds18b20_get_celsius(&dev, &celsius) == ROBOHUB_OK && near(celsius, -55.0f, 0.0001f),
          "ds18b20: 0xFC90 is -55 C");

    ow_set_scratchpad(0x0191u, 0x7Fu, RH_FALSE);
    CHECK(robohub_ds18b20_get_celsius(&dev, &celsius) == ROBOHUB_ERR_PROTOCOL,
          "ds18b20: a scratchpad whose CRC does not check is a protocol error");

    /* 9 bits: the three low bits are undefined and dropped. */
    config.resolution_bits = 9u;
    ow_set_scratchpad(0x0197u, 0x1Fu, RH_TRUE);
    CHECK(robohub_ds18b20_init(&dev, &config) == ROBOHUB_OK, "ds18b20: init at 9 bits");
    CHECK(robohub_ds18b20_get_celsius(&dev, &celsius) == ROBOHUB_OK && near(celsius, 25.0f, 0.0001f),
          "ds18b20: at 9 bits 0x0197 reads as 25.0 C");

    ow_present = RH_FALSE;
    config.resolution_bits = 0u;
    CHECK(robohub_ds18b20_init(&dev, &config) == ROBOHUB_ERR_NOT_FOUND,
          "ds18b20: a bus with nothing on it is not found");
}

int main(void)
{
    RoboHubTimer timer = { .name = "system_timer" };
    timer.get_millis = timer_ms;
    timer.delay_ms = timer_delay_ms;

    test_bmp180(&timer);
    test_dht11(&timer);
    test_ds18b20(&timer);

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all environmental checks passed\n");
    return 0;
}
