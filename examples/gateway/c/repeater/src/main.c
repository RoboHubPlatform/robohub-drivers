/**
 * @file main.c
 * @brief A Pico 2W gateway: it measures, and it repeats whatever it hears.
 *
 * The same stack as the telemetry node, with one more link:
 *
 *   robohub-drivers   the local DS18B20 publishes EVENT_TEMP_READING
 *   the downstream    frames that arrive on uart1 publish the same event
 *   robohub-core      carries either to its subscribers
 *   robohub-telemetry subscribed to that event, writes each to the upstream
 *   robohub-transport frames it as COBS on the USB CDC link
 *
 * Nothing forwards a board by hand. A reading names its node, so the bus needs
 * no source table: the local sensor and every downstream frame are the same
 * event, and the bridge that carries one carries all. A Pico with a UART
 * transceiver is a gateway for the boards behind it and a node in its own right
 * at once; with the host on the USB CDC it is a repeater, and without one it is
 * the gateway, streaming to whatever listens.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "pico/stdlib.h"

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "error.h"
#include "robohub/hal/console.h"
#include "robohub/hal/onewire.h"
#include "robohub/hal/timer.h"
#include "robohub/hal/uart.h"
#include "pico_uart.h"
#include "robohub/drivers/temperature/ds18b20.h"
#include "robohub/core/core.h"
#include "robohub/core/app.h"
#include "robohub/core/event.h"
#include "robohub/core/scheduler.h"
#include "robohub/transport.h"
#include "robohub/telemetry.h"
#include "protocol.h"

/* The names the Pico's board support package registers. */
#define ONEWIRE_NAME "onewire0"
#define TIMER_NAME   "system_timer"
#define DOWNSTREAM_UART "uart1"
#define DOWNSTREAM_BAUD 115200u
#define REPORT_PERIOD_MS 500u

/* The event a reading travels as, and its layout, both from the schema. */
#define EVENT_TEMP_READING MESSAGE_ID_TELEMETRY

/* This board's identity on the bus. */
#if defined(CONFIG_RH_NODE_ID)
#define RH_NODE_ID ((rh_u16)CONFIG_RH_NODE_ID)
#else
#define RH_NODE_ID ((rh_u16)1u)
#endif

static void emit(const char* text)
{
    (void)robohub_console_puts(text);
}

/* -------------------------------------------------------------------------- */
/*  The two links                                                             */
/* -------------------------------------------------------------------------- */

/* Upstream: the USB CDC, written raw because a frame is bytes, not a line. The
 * console text a program prints goes out the same port. */
static RoboHubTransport upstream;

static int upstream_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    return (robohub_console_write_raw(data, length) == ROBOHUB_CONSOLE_OK) ? 0 : -1;
}

/* Downstream: the UART a board is wired to. */
static RoboHubTransport downstream;
static RoboHubUART* down_link;

static int downstream_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    if (down_link == 0) {
        return -1;
    }
    return (down_link->write(down_link, data, length) == ROBOHUB_OK) ? 0 : -1;
}

/* -------------------------------------------------------------------------- */
/*  Sensor application                                                        */
/* -------------------------------------------------------------------------- */

static RoboHubDS18B20 sensor;
static RoboHubTimer* system_timer;
static robohub_core_task_id sample_task = RH_CORE_TASK_INVALID;
static rh_u16 sample_period_ms = REPORT_PERIOD_MS;

static RoboHubError read_and_publish(robohub_core_task_id id, void* user)
{
    (void)id;
    (void)user;

    rh_f32 celsius = 0.0f;
    RoboHubError err = robohub_ds18b20_get_celsius(&sensor, &celsius);

    Telemetry message = {
        .node_id = RH_NODE_ID,
        .timestamp_ms = to_ms_since_boot(get_absolute_time()),
        .temperature_c = (ROBOHUB_SUCCEEDED(err)) ? celsius : 0.0f,
        .status = (ROBOHUB_SUCCEEDED(err)) ? NODE_STATUS_OK : NODE_STATUS_FAULT,
    };
    rh_u8 payload[TELEMETRY_WIRE_SIZE];
    if (Telemetry_serialize(&message, payload, sizeof(payload)) == 0u) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    return robohub_core_event_publish(EVENT_TEMP_READING, payload, (rh_size)TELEMETRY_WIRE_SIZE);
}

/* A sampling-rate command addressed to this board. A command for a downstream
 * board is not routed here: the gateway's own period is what it can change. */
static RoboHubError on_set_sampling_rate(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;

    SetSamplingRateRequest request;
    if (!SetSamplingRateRequest_deserialize((const uint8_t*)payload, (size_t)size, &request)) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    rh_u32 interval = request.interval_ms;
    if (interval < 100u) {
        interval = 100u;
    }
    if (ROBOHUB_SUCCEEDED(robohub_core_task_set_period(sample_task, interval))) {
        sample_period_ms = (rh_u16)interval;
    }

    SetSamplingRateResponse response = { .actual_interval_ms = sample_period_ms };
    rh_u8 bytes[SET_SAMPLING_RATE_RESPONSE_WIRE_SIZE];
    if (SetSamplingRateResponse_serialize(&response, bytes, sizeof(bytes)) == 0u) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    return robohub_core_event_publish_with_tx(MESSAGE_ID_SET_SAMPLING_RATE_RESPONSE,
                                              robohub_core_event_current_tx(),
                                              bytes, (rh_size)SET_SAMPLING_RATE_RESPONSE_WIRE_SIZE);
}

static RoboHubError gateway_init(void)
{
    RoboHubOneWire* bus = robohub_get_onewire(ONEWIRE_NAME);
    system_timer = robohub_get_timer(TIMER_NAME);
    down_link = robohub_get_uart(DOWNSTREAM_UART);
    if (bus == 0 || system_timer == 0 || down_link == 0) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    if (ROBOHUB_FAILED(down_link->init(down_link, DOWNSTREAM_BAUD))) {
        return ROBOHUB_ERR_IO;
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

static RoboHubError gateway_start(void)
{
    return robohub_core_task_add("temp", read_and_publish, 0, sample_period_ms, &sample_task);
}

/* Every pass: move bytes from each link into its transport. A byte completed
 * into a frame publishes on the bus, so a downstream reading and a local one
 * take the same path to the upstream sink. */
static void pump(rh_u32 now_ms)
{
    (void)now_ms;

    if (down_link != 0) {
        rh_u8 buf[64];
        rh_size got = pico_uart_read_available(down_link, buf, (rh_size)sizeof(buf));
        if (got > 0u) {
            robohub_transport_feed(&downstream, buf, got);
        }
    }

    for (int i = 0; i < 64; i++) {
        int byte = robohub_console_read_byte();
        if (byte < 0) {
            break;
        }
        robohub_transport_feed_byte(&upstream, (rh_u8)byte);
    }
}

static const robohub_core_app gateway_app = {
    .name = "gateway",
    .init = gateway_init,
    .start = gateway_start,
    .step = pump,
};

/* -------------------------------------------------------------------------- */
/*  Entry                                                                     */
/* -------------------------------------------------------------------------- */

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        for (;;) {
        }
    }

    (void)robohub_telemetry_init();

    /* Upstream on the USB CDC, downstream on the UART the boards sit behind. */
    const RoboHubTransportPhy up_phy = { .write = upstream_write, .context = 0 };
    (void)robohub_transport_init(&upstream, &up_phy);
    const RoboHubTransportPhy down_phy = { .write = downstream_write, .context = 0 };
    (void)robohub_transport_init(&downstream, &down_phy);

    RoboHubTelemetrySink sink = {
        .type = ROBOHUB_SINK_TRANSPORT,
        .handle.transport = &upstream,
        .enabled = RH_TRUE,
    };
    (void)robohub_telemetry_add_sink(&sink);

    (void)robohub_core_app_register(&gateway_app);
    if (ROBOHUB_FAILED(robohub_core_init(0))) {
        emit("gateway failed to start; is the sensor and the downstream link wired?\n");
        for (;;) {
            if (system_timer != 0) {
                system_timer->delay_ms(system_timer, 1000u);
            }
        }
    }

    /* Every reading — this board's and every board's behind it — streams up. */
    (void)robohub_telemetry_enable_event(EVENT_TEMP_READING, 1u);
    (void)robohub_telemetry_enable_event(MESSAGE_ID_SET_SAMPLING_RATE_RESPONSE, 1u);
    (void)robohub_core_event_subscribe(MESSAGE_ID_SET_SAMPLING_RATE_REQUEST, on_set_sampling_rate, 0);

    (void)robohub_core_run();
    for (;;) {
    }
}
