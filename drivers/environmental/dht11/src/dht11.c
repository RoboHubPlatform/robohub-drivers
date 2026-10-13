/*
 * dht11.c — the Aosong DHT11 temperature and humidity sensor.
 *
 * Everything here is the frame and its arithmetic. The wire — the start pulse
 * and the 40 measured pulses — belongs to the `RoboHubDHT` bus a platform
 * implements, and the pacing to the `RoboHubTimer` when the caller bound one.
 * This file names neither a chip nor a pin, which is what lets it be compiled
 * for any board that has the bus.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "dht11.h"
#include "robohub/drivers/temperature_utils.h"

#include <stddef.h>
#include <string.h>

/* The frame: humidity integer and decimal, temperature integer and decimal,
 * checksum. The checksum is the low byte of the sum of the first four. */
#define DHT11_BYTE_HUMIDITY_INT 0u
#define DHT11_BYTE_HUMIDITY_DEC 1u
#define DHT11_BYTE_TEMP_INT     2u
#define DHT11_BYTE_TEMP_DEC     3u
#define DHT11_BYTE_CHECKSUM     4u

/* The sign bit of the temperature decimal byte. A DHT22 sets it for a
 * temperature below zero; a DHT11 never does, and reading it costs nothing. */
#define DHT11_TEMP_SIGN_BIT     0x80u

/* -------------------------------------------------------------------------- */
/*  Helpers                                                                   */
/* -------------------------------------------------------------------------- */

/* Wait out the part's recovery time, and remember when a measurement was taken
 * so the next one is paced. Without a timer there is nothing to wait with, and
 * pacing is the caller's. */
static void dht11_pace(RoboHubDHT11* dev)
{
    if (dev->timer == NULL || dev->timer->get_millis == NULL) {
        return;
    }
    if (dev->have_last && dev->timer->delay_ms != NULL) {
        rh_millis now = dev->timer->get_millis(dev->timer);
        rh_millis elapsed = now - dev->last_read_ms;
        if (elapsed < dev->min_interval_ms) {
            dev->timer->delay_ms(dev->timer, (rh_u32)(dev->min_interval_ms - elapsed));
        }
    }
}

static void dht11_mark_read(RoboHubDHT11* dev)
{
    if (dev->timer != NULL && dev->timer->get_millis != NULL) {
        dev->last_read_ms = dev->timer->get_millis(dev->timer);
        dev->have_last = RH_TRUE;
    }
}

/* Bring the part up: the bus, and the first read paced from now so the part's
 * power-up delay has passed before it is asked anything. */
static RoboHubError dht11_bring_up(RoboHubDHT11* dev)
{
    if (dev->bus == NULL || dev->bus->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (dev->bus->init != NULL) {
        RoboHubError err = dev->bus->init(dev->bus);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
    }
    dev->ready = RH_TRUE;
    dht11_mark_read(dev);
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Reading                                                                   */
/* -------------------------------------------------------------------------- */

static RoboHubError dht11_read_frame(RoboHubDHT11* dev, rh_f32* celsius, rh_f32* percent)
{
    if (!dev->ready) {
        return ROBOHUB_ERR_STATE;
    }

    dht11_pace(dev);

    rh_u8 frame[ROBOHUB_DHT_FRAME_BYTES];
    RoboHubError err = dev->bus->read(dev->bus, frame, ROBOHUB_DHT_FRAME_BYTES);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_u8 sum = (rh_u8)(frame[DHT11_BYTE_HUMIDITY_INT] + frame[DHT11_BYTE_HUMIDITY_DEC] +
                        frame[DHT11_BYTE_TEMP_INT] + frame[DHT11_BYTE_TEMP_DEC]);
    if (sum != frame[DHT11_BYTE_CHECKSUM]) {
        return ROBOHUB_ERR_PROTOCOL;
    }

    if (percent != NULL) {
        *percent = (rh_f32)frame[DHT11_BYTE_HUMIDITY_INT] +
                   (rh_f32)frame[DHT11_BYTE_HUMIDITY_DEC] / 10.0f;
    }

    if (celsius != NULL) {
        rh_u8 dec = frame[DHT11_BYTE_TEMP_DEC];
        rh_f32 value = (rh_f32)frame[DHT11_BYTE_TEMP_INT] +
                       (rh_f32)(dec & (rh_u8)~DHT11_TEMP_SIGN_BIT) / 10.0f;
        if ((dec & DHT11_TEMP_SIGN_BIT) != 0u) {
            value = -value;
        }
        *celsius = value;
    }

    dht11_mark_read(dev);
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Temperature interface                                                     */
/* -------------------------------------------------------------------------- */

static RoboHubError dht11_temp_init(RoboHubTemperature* self)
{
    return dht11_bring_up((RoboHubDHT11*)self->context);
}

static RoboHubError dht11_temp_deinit(RoboHubTemperature* self)
{
    ((RoboHubDHT11*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError dht11_temp_celsius(RoboHubTemperature* self, rh_f32* celsius)
{
    if (celsius == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return dht11_read_frame((RoboHubDHT11*)self->context, celsius, NULL);
}

static RoboHubError dht11_temp_fahrenheit(RoboHubTemperature* self, rh_f32* fahrenheit)
{
    if (fahrenheit == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rh_f32 celsius = 0.0f;
    RoboHubError err = dht11_read_frame((RoboHubDHT11*)self->context, &celsius, NULL);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    *fahrenheit = robohub_celsius_to_fahrenheit(celsius);
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Humidity interface                                                        */
/* -------------------------------------------------------------------------- */

static RoboHubError dht11_hum_init(RoboHubHumidity* self)
{
    return dht11_bring_up((RoboHubDHT11*)self->context);
}

static RoboHubError dht11_hum_deinit(RoboHubHumidity* self)
{
    ((RoboHubDHT11*)self->context)->ready = RH_FALSE;
    return ROBOHUB_OK;
}

static RoboHubError dht11_hum_get(RoboHubHumidity* self, rh_f32* percent)
{
    if (percent == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return dht11_read_frame((RoboHubDHT11*)self->context, NULL, percent);
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_dht11_init(RoboHubDHT11* dev, const RoboHubDHT11Config* config)
{
    if (dev == NULL || config == NULL || config->bus == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->bus = config->bus;
    dev->timer = config->timer;
    dev->min_interval_ms = config->min_interval_ms != 0u
        ? config->min_interval_ms
        : ROBOHUB_DHT11_MIN_INTERVAL_MS;
    dev->ready = RH_FALSE;

    dev->temperature.name = "dht11.temperature";
    dev->temperature.context = dev;
    dev->temperature.init = dht11_temp_init;
    dev->temperature.deinit = dht11_temp_deinit;
    dev->temperature.get_celsius = dht11_temp_celsius;
    dev->temperature.get_fahrenheit = dht11_temp_fahrenheit;

    dev->humidity.name = "dht11.humidity";
    dev->humidity.context = dev;
    dev->humidity.init = dht11_hum_init;
    dev->humidity.deinit = dht11_hum_deinit;
    dev->humidity.get_humidity = dht11_hum_get;

    return dht11_bring_up(dev);
}

RoboHubError robohub_dht11_deinit(RoboHubDHT11* dev)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dev->ready = RH_FALSE;
    if (dev->bus != NULL && dev->bus->deinit != NULL) {
        return dev->bus->deinit(dev->bus);
    }
    return ROBOHUB_OK;
}

RoboHubError robohub_dht11_get_celsius(RoboHubDHT11* dev, rh_f32* celsius)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return dht11_temp_celsius(&dev->temperature, celsius);
}

RoboHubError robohub_dht11_get_fahrenheit(RoboHubDHT11* dev, rh_f32* fahrenheit)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return dht11_temp_fahrenheit(&dev->temperature, fahrenheit);
}

RoboHubError robohub_dht11_get_humidity(RoboHubDHT11* dev, rh_f32* percent)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return dht11_hum_get(&dev->humidity, percent);
}

RoboHubError robohub_dht11_read(RoboHubDHT11* dev, rh_f32* celsius, rh_f32* percent)
{
    if (dev == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (celsius == NULL && percent == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return dht11_read_frame(dev, celsius, percent);
}
