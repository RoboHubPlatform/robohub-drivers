/**
 * @file main.c
 * @brief ds18b20 — read the DS18B20 module and print it, on the runtime.
 *
 * The same application as before, restructured so the sensor and the console do
 * not call each other. A sensor application reads the part and publishes a
 * reading; a telemetry application subscribes to it and prints it. Neither
 * names the other, and the sensor could be replaced without the telemetry
 * changing — which is the whole reason the runtime exists.
 *
 *     platform init  ->  register apps  ->  core init  ->  core run
 *
 * The runtime owns the loop, so this file has no loop of its own: it declares
 * what the parts are and lets the runtime bring them up and run them.
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
#include "robohub/drivers/temperature/ds18b20.h"
#include "robohub/core/core.h"
#include "robohub/core/app.h"
#include "robohub/core/event.h"
#include "robohub/core/events.h"
#include "robohub/core/scheduler.h"
#include "robohub/core/diagnostics.h"

/* The names the Pico's board support package registers its bus and timer under;
 * the same names the module's own BSP defaults to. */
#define ONEWIRE_NAME "onewire0"
#define TIMER_NAME   "system_timer"

#define REPORT_PERIOD_MS     1500u
#define HEARTBEAT_PERIOD_MS  5000u

/* A sensor event: what it means is the sensor layer's to say. */
#define EVENT_TEMP_READING RH_EVENT_ID(RH_EVENT_DOMAIN_SENSOR, 1)

/**
 * @brief What a temperature reading is.
 *
 * The payload of the event both applications agree on. Zero-copy means the
 * subscriber is handed this struct in place, so it must read it during the
 * call and not keep the pointer.
 */
typedef struct {
    rh_f32 celsius;
} temperature_reading;

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

/* Degrees with three decimals, built by hand so the console does not need the
 * C library's floating-point formatting pulled in. */
static void emit_celsius(rh_f32 celsius)
{
    int32_t milli = (int32_t)(celsius * 1000.0f + (celsius >= 0.0f ? 0.5f : -0.5f));
    int32_t whole = milli / 1000;
    int32_t fraction = milli % 1000;
    if (fraction < 0) {
        fraction = -fraction;
    }

    emit_i32(whole);
    emit(".");
    if (fraction < 100) {
        emit("0");
    }
    if (fraction < 10) {
        emit("0");
    }
    emit_i32(fraction);
}

/* -------------------------------------------------------------------------- */
/*  Sensor application                                                        */
/* -------------------------------------------------------------------------- */
/* Owns the device and nothing else: it reads on a period and publishes what it
 * read. It has never heard of the console. */

static RoboHubDS18B20 sensor;
static RoboHubTimer* system_timer;

static RoboHubError read_and_publish(robohub_core_task_id id, void* user)
{
    (void)id;
    (void)user;

    rh_f32 celsius = 0.0f;
    RoboHubError err = robohub_ds18b20_get_celsius(&sensor, &celsius);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    /* The reading is a local, and its address is handed to the bus. That is the
     * zero-copy contract: every handler reads it before this returns. */
    temperature_reading reading = { .celsius = celsius };
    return robohub_core_event_publish(EVENT_TEMP_READING, &reading, (rh_size)sizeof(reading));
}

static RoboHubError sensor_init(void)
{
    RoboHubOneWire* bus = robohub_get_onewire(ONEWIRE_NAME);
    system_timer = robohub_get_timer(TIMER_NAME);
    if (bus == 0 || system_timer == 0) {
        return ROBOHUB_ERR_NOT_FOUND;
    }

    RoboHubDS18B20Config config = {
        .bus = bus,
        .timer = system_timer,
        .resolution_bits = 12,
        .alarm_high_c = 30,
        .alarm_low_c = 0,
    };

    RoboHubError err = ROBOHUB_ERR_IO;
    for (rh_u32 attempt = 0; attempt < 5u; attempt++) {
        err = robohub_ds18b20_init(&sensor, &config);
        if (ROBOHUB_SUCCEEDED(err)) {
            return ROBOHUB_OK;
        }
        system_timer->delay_ms(system_timer, 200u);
    }
    return err;
}

static RoboHubError sensor_start(void)
{
    return robohub_core_task_add("temp", read_and_publish, 0, REPORT_PERIOD_MS, 0);
}

static const robohub_core_app sensor_app = {
    .name = "sensor",
    .init = sensor_init,
    .start = sensor_start,
};

/* -------------------------------------------------------------------------- */
/*  Telemetry application                                                     */
/* -------------------------------------------------------------------------- */
/* Subscribes to the reading and prints it. It has never heard of the sensor. */

static RoboHubError on_temperature(robohub_event_id id, const void* payload,
                                   rh_size size, void* user)
{
    (void)id;
    (void)user;
    if (payload == 0 || size != (rh_size)sizeof(temperature_reading)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    const temperature_reading* reading = (const temperature_reading*)payload;
    emit("T = ");
    emit_celsius(reading->celsius);
    emit(" C\n");
    return ROBOHUB_OK;
}

static RoboHubError report_health(robohub_core_task_id id, void* user)
{
    (void)id;
    (void)user;

    robohub_core_diagnostics diag;
    robohub_core_diagnostics_get(&diag);

    emit("[");
    emit_i32((int32_t)(diag.uptime_ms / 1000u));
    emit(" s] events=");
    emit_i32((int32_t)diag.events_published);
    emit(" deliveries=");
    emit_i32((int32_t)diag.event_deliveries);
    emit(" tasks=");
    emit_i32((int32_t)diag.tasks_run);
    emit(" errors=");
    emit_i32((int32_t)(diag.handler_errors + diag.task_errors));
    emit("\n");
    return ROBOHUB_OK;
}

static RoboHubError telemetry_init(void)
{
    emit("\nDS18B20 temperature, on the RoboHub runtime\n");
    emit("-------------------------------------------\n");
    return robohub_core_event_subscribe(EVENT_TEMP_READING, on_temperature, 0);
}

static RoboHubError telemetry_start(void)
{
    return robohub_core_task_add("health", report_health, 0, HEARTBEAT_PERIOD_MS, 0);
}

static const robohub_core_app telemetry_app = {
    .name = "telemetry",
    .init = telemetry_init,
    .start = telemetry_start,
};

/* -------------------------------------------------------------------------- */
/*  Entry                                                                     */
/* -------------------------------------------------------------------------- */

/* Keep the board alive without running its modules, so the console it enumerates
 * on stays enumerated and says what is wrong. Returning from main would leave
 * the host with a failed enumeration instead. */
static void halt_with(const char* message)
{
    for (;;) {
        emit(message);
        emit("\n");
        if (system_timer != 0 && system_timer->delay_ms != 0) {
            system_timer->delay_ms(system_timer, 2000u);
        }
    }
}

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        for (;;) {
        }
    }

    (void)robohub_core_app_register(&sensor_app);
    (void)robohub_core_app_register(&telemetry_app);

    RoboHubError err = robohub_core_init(0);
    if (ROBOHUB_FAILED(err)) {
        halt_with("runtime failed to start; is a 1-Wire bus and the sensor up?");
    }

    err = robohub_core_run();
    if (ROBOHUB_FAILED(err)) {
        halt_with("runtime stopped with an error");
    }

    for (;;) {
    }
}
