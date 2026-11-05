/**
 * @file sensors.c
 * @brief The default node sensor set: one DS18B20 on the module bus.
 *
 * Mesh replaces this file when it builds a node from a plan, generating one
 * block per fitted module. This version keeps the DS18B20 example a working
 * program on its own: open the module, read it, and publish the reading through
 * the generic SensorReading that the gateway stores by field name.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico/stdlib.h"

#include <string.h>

#include "types.h"
#include "error.h"
#include "robohub/core/core.h"
#include "robohub/core/event.h"
#include "robohub/core/events.h"
#include "ds18b20_temperature_module.h"
#include "protocol.h"

#if defined(CONFIG_RH_AREA_ID)
#define RH_AREA_ID ((rh_u16)CONFIG_RH_AREA_ID)
#else
#define RH_AREA_ID ((rh_u16)1u)
#endif
#if defined(CONFIG_RH_NODE_ID)
#define RH_NODE_ID ((rh_u16)CONFIG_RH_NODE_ID)
#else
#define RH_NODE_ID ((rh_u16)1u)
#endif

static RoboHubDS18B20 sensor;
static rh_bool sensor_ready;
static RoboHubError sensor_error = ROBOHUB_ERR_NOT_FOUND;
static rh_u32 next_attempt_ms;

static void publish_reading(const char* field, rh_f32 value, rh_u16 status)
{
    SensorReading reading = {
        .device = { .area_id = RH_AREA_ID, .node_id = RH_NODE_ID },
        .timestamp_ms = to_ms_since_boot(get_absolute_time()),
        .status = status,
        .value = value,
    };
    strncpy(reading.field, field, sizeof(reading.field) - 1);
    rh_u8 payload[SENSOR_READING_WIRE_SIZE];
    if (SensorReading_serialize(&reading, payload, sizeof(payload)) == 0u) {
        return;
    }
    (void)robohub_core_event_publish(MESSAGE_ID_SENSOR_READING, payload,
                                     (rh_size)SENSOR_READING_WIRE_SIZE);
}

void sensors_init(void)
{
    sensor_error = robohub_ds18b20_module_init(&sensor, NULL);
    sensor_ready = ROBOHUB_SUCCEEDED(sensor_error);
    next_attempt_ms = 0u;
}

void sensors_poll(void)
{
    rh_u32 now_ms = to_ms_since_boot(get_absolute_time());
    if (!sensor_ready && now_ms >= next_attempt_ms) {
        next_attempt_ms = now_ms + 5000u;
        sensor_error = robohub_ds18b20_module_init(&sensor, NULL);
        sensor_ready = ROBOHUB_SUCCEEDED(sensor_error);
    }

    rh_f32 celsius = 0.0f;
    RoboHubError err = ROBOHUB_ERR_NOT_FOUND;
    if (sensor_ready) {
        err = sensor.interface.get_celsius(&sensor.interface, &celsius);
        if (ROBOHUB_FAILED(err)) {
            sensor_error = err;
            sensor_ready = RH_FALSE;
            next_attempt_ms = now_ms + 1000u;
        } else {
            sensor_error = ROBOHUB_OK;
        }
    }

    publish_reading("temperature_c",
                    ROBOHUB_SUCCEEDED(err) ? celsius : 0.0f,
                    ROBOHUB_SUCCEEDED(err) ? NODE_STATUS_OK : NODE_STATUS_FAULT);
}

RoboHubError sensors_last_error(void)
{
    return sensor_error;
}
