/**
 * @file main.c
 * @brief The whole stack: a sensor publishes, telemetry streams it over USB CDC.
 *
 * Three modules meet here and none of them knows the others:
 *
 *   robohub-drivers   the DS18B20 publishes EVENT_TEMP_READING
 *   robohub-core      carries the event to its subscribers
 *   robohub-telemetry subscribed to that event, writes each reading to a sink
 *   robohub-transport frames each write as COBS and hands it to the link
 *   the link          this board's USB CDC, one raw byte at a time
 *
 * The program itself does nothing but declare that the event is worth
 * streaming. The console carries binary frames, not text: a reading is a packed
 * struct inside a COBS frame, and a host decodes it — which is the point, since
 * the same bytes are what a gateway parses.
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
#include "robohub/hal/uart.h"
#include "robohub/hal/onewire.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/temperature/ds18b20.h"
#include "robohub/core/core.h"
#include "robohub/core/app.h"
#include "robohub/core/event.h"
#include "robohub/core/events.h"
#include "robohub/core/diagnostics.h"
#include "robohub/core/scheduler.h"
#include "robohub/transport.h"
#include "robohub/telemetry.h"
#include "protocol.h"

#if defined(__has_include)
#  if __has_include("autoconf.h")
#    include "autoconf.h"
#  endif
#endif
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
#include "robohub/net.h"
#endif

/* The names the Pico's board support package registers; the module's defaults. */
#define ONEWIRE_NAME "onewire0"
#define TIMER_NAME   "system_timer"
#define REPORT_PERIOD_MS 500u

/* When the network is enabled, the same frames also go to a TCP gateway.
 *
 * No credential is committed here: the SSID and password are empty unless the
 * build supplies them (`-DRH_WIFI_SSID=... -DRH_WIFI_PASSWORD=...`, or what a
 * deployment carries as configuration). With an empty SSID the network sink is
 * simply not started, so the example still builds and runs over USB alone. */
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
#ifndef RH_WIFI_SSID
#  if defined(CONFIG_RH_WIFI_SSID)
#    define RH_WIFI_SSID CONFIG_RH_WIFI_SSID
#  else
#    define RH_WIFI_SSID ""
#  endif
#endif
#ifndef RH_WIFI_PASSWORD
#  if defined(CONFIG_RH_WIFI_PASSWORD)
#    define RH_WIFI_PASSWORD CONFIG_RH_WIFI_PASSWORD
#  else
#    define RH_WIFI_PASSWORD ""
#  endif
#endif
#ifndef RH_TELEMETRY_HOST
#  if defined(CONFIG_RH_TELEMETRY_HOST)
#    define RH_TELEMETRY_HOST CONFIG_RH_TELEMETRY_HOST
#  else
#    define RH_TELEMETRY_HOST "192.168.1.10"
#  endif
#endif
#ifndef RH_TELEMETRY_PORT
#  if defined(CONFIG_RH_TELEMETRY_PORT)
#    define RH_TELEMETRY_PORT CONFIG_RH_TELEMETRY_PORT
#  else
#    define RH_TELEMETRY_PORT 5000
#  endif
#endif
#endif

#ifndef RH_NODE_TRANSPORT
#  if defined(CONFIG_RH_NODE_TRANSPORT)
#    define RH_NODE_TRANSPORT CONFIG_RH_NODE_TRANSPORT
#  else
#    define RH_NODE_TRANSPORT "usb"
#  endif
#endif
#ifndef RH_NODE_UART
#  if defined(CONFIG_RH_NODE_UART)
#    define RH_NODE_UART CONFIG_RH_NODE_UART
#  else
#    define RH_NODE_UART "uart1"
#  endif
#endif
#ifndef RH_NODE_BAUD
#  if defined(CONFIG_RH_NODE_BAUD)
#    define RH_NODE_BAUD CONFIG_RH_NODE_BAUD
#  else
#    define RH_NODE_BAUD 115200
#  endif
#endif

/* The event the reading travels as, and its layout, both from the schema. The
 * gateway is generated from the same schemas/link.rho, so the two ends write one
 * layout; the schema names the reading MESSAGE_ID_TELEMETRY (0x0201).
 *
 * The payload is the schema's `Telemetry` — a version byte, the device (an area
 * and a node), timestamp_ms, temperature_c and status — serialized here rather
 * than the bare float this example once published. A gateway that decoded the
 * bare float could not be sure what it was; the schema's message it can. */
#define EVENT_TEMP_READING MESSAGE_ID_TELEMETRY

/* This board's identity on the bus: the area it is in and its node id within it.
 * A gateway numbers several areas, so the pair — not the node id alone — is what
 * names a device. */
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

/* Sampling rate is control opcode 1; the control family is general, so a second
 * command is another opcode rather than another message. */
#define CONTROL_SAMPLING_RATE 1u

static void emit(const char* text)
{
    (void)robohub_console_puts(text);
}

/* -------------------------------------------------------------------------- */
/*  The link: the transport the network step chose                            */
/* -------------------------------------------------------------------------- */

static RoboHubTransport node_link;
static RoboHubUART* node_uart;
static rh_bool link_is_net;

static int usb_phy_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    /* putchar_raw, not printf: a frame is bytes, and the C library's line
     * buffering would hold them and then insert its own. */
    for (rh_size i = 0; i < length; i++) {
        (void)putchar_raw((int)data[i]);
    }
    return 0;
}

static int uart_phy_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    if (node_uart == 0 || node_uart->write == 0) {
        return -1;
    }
    return (node_uart->write(node_uart, data, length) == ROBOHUB_OK) ? 0 : -1;
}

/* -------------------------------------------------------------------------- */
/*  Sensor application                                                        */
/* -------------------------------------------------------------------------- */

static RoboHubDS18B20 sensor;
static RoboHubDS18B20Config sensor_config;
static RoboHubTimer* system_timer;
static rh_u32 next_sensor_attempt_ms;
static RoboHubError sensor_error = ROBOHUB_OK;

/* A board whose sensor did not answer still reports: the runtime starts, and
 * every reading carries FAULT. A gateway then says "this device is there and
 * its sensor is not", which is more useful than a device that is simply absent
 * because its init refused to let the runtime run. */
static rh_bool sensor_ready = RH_FALSE;

static RoboHubError read_and_publish(robohub_core_task_id id, void* user)
{
    (void)id;
    (void)user;

    /* A sensor connected after boot is still picked up: the one-time init at
     * startup is retried every few seconds while the bus says nothing. Without
     * this, plugging the part in later leaves the node reporting FAULT until it
     * is reset, even though the bus and the part are fine. */
    if (!sensor_ready && sensor_config.bus != 0 && system_timer != 0) {
        rh_u32 now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms >= next_sensor_attempt_ms) {
            next_sensor_attempt_ms = now_ms + 5000u;
            sensor_error = robohub_ds18b20_init(&sensor, &sensor_config);
            if (ROBOHUB_SUCCEEDED(sensor_error)) {
                sensor_ready = RH_TRUE;
            }
        }
    }

    rh_f32 celsius = 0.0f;
    RoboHubError err = ROBOHUB_ERR_NOT_FOUND;
    if (sensor_ready) {
        err = robohub_ds18b20_get_celsius(&sensor, &celsius);
        if (ROBOHUB_FAILED(err)) {
            sensor_error = err;
            sensor_ready = RH_FALSE;
            next_sensor_attempt_ms = to_ms_since_boot(get_absolute_time()) + 1000u;
        } else {
            sensor_error = ROBOHUB_OK;
        }
    }

    /* A failed read is still published, carrying FAULT: a gateway that sees the
     * status knows the difference between a stale reading and a dead sensor,
     * which a missing frame could not tell it. */
    Telemetry message = {
        .device = { .area_id = RH_AREA_ID, .node_id = RH_NODE_ID },
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

static RoboHubError sensor_init(void)
{
    RoboHubOneWire* bus = robohub_get_onewire(ONEWIRE_NAME);
    system_timer = robohub_get_timer(TIMER_NAME);
    if (bus == 0 || system_timer == 0) {
        sensor_error = ROBOHUB_ERR_NOT_FOUND;
        return ROBOHUB_OK;
    }
    sensor_config.bus = bus;
    sensor_config.timer = system_timer;
    sensor_config.resolution_bits = 12;
    sensor_config.alarm_high_c = 30;
    sensor_config.alarm_low_c = 0;
    next_sensor_attempt_ms = 0u;
    RoboHubError err = ROBOHUB_ERR_IO;
    for (rh_u32 attempt = 0; attempt < 5u; attempt++) {
        err = robohub_ds18b20_init(&sensor, &sensor_config);
        if (ROBOHUB_SUCCEEDED(err)) {
            sensor_ready = RH_TRUE;
            sensor_error = ROBOHUB_OK;
            return ROBOHUB_OK;
        }
        system_timer->delay_ms(system_timer, 200u);
    }
    /* The sensor is absent or not answering. That is a fault to report, not a
     * reason to leave the board without a runtime. */
    sensor_ready = RH_FALSE;
    sensor_error = err;
    return ROBOHUB_OK;
}

static robohub_core_task_id sample_task = RH_CORE_TASK_INVALID;
static rh_u16 sample_period_ms = REPORT_PERIOD_MS;

static RoboHubError sensor_start(void)
{
    return robohub_core_task_add("temp", read_and_publish, 0, sample_period_ms, &sample_task);
}

/* A control from the gateway: `CONTROL_REQUEST` in, `CONTROL_RESPONSE` out, the
 * caller's tx_id carried back so it can match the answer to its request. The
 * request names the device it is for and carries one argument; sampling rate is
 * opcode 1, and the period is clamped to one a DS18B20 conversion can meet. The
 * acknowledgement repeats the device and the opcode and reports what was
 * actually set, not what was asked for. */
static RoboHubError on_control(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;

    ControlRequest request;
    if (!ControlRequest_deserialize((const uint8_t*)payload, (size_t)size, &request)) {
        return ROBOHUB_ERR_PROTOCOL;
    }

    ControlResponse response = {
        .device = request.device,
        .opcode = request.opcode,
        .result = 0u,
        .argument = request.argument,
    };

    if (request.opcode != CONTROL_SAMPLING_RATE) {
        response.result = 2u; /* no such command */
    } else {
        rh_u32 interval = request.argument;
        if (interval < 100u) {
            interval = 100u;
        }
        if (ROBOHUB_FAILED(robohub_core_task_set_period(sample_task, interval))) {
            response.result = 1u;
        } else {
            sample_period_ms = (rh_u16)interval;
            response.argument = sample_period_ms;
        }
    }

    rh_u8 bytes[CONTROL_RESPONSE_WIRE_SIZE];
    if (ControlResponse_serialize(&response, bytes, sizeof(bytes)) == 0u) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    return robohub_core_event_publish_with_tx(MESSAGE_ID_CONTROL_RESPONSE,
                                              robohub_core_event_current_tx(),
                                              bytes, (rh_size)CONTROL_RESPONSE_WIRE_SIZE);
}

/* A diagnostic request: run the smoke the mask asks for and answer with the
 * runtime's counters and the result. A gateway reads this to tell a board that
 * stopped from a board whose OneWire bus was never up. */
static RoboHubError on_diag(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;

    DiagRequest request;
    if (!DiagRequest_deserialize((const uint8_t*)payload, (size_t)size, &request)) {
        return ROBOHUB_ERR_PROTOCOL;
    }

    robohub_core_diagnostics counters;
    robohub_core_diagnostics_get(&counters);

    rh_u32 probe_ok = 0u;
    rh_u32 probe_fault = 0u;
    (void)robohub_core_smoke(system_timer, request.mask, &probe_ok, &probe_fault);

    DiagResponse response = {
        .device = request.device,
        .uptime_ms = counters.uptime_ms,
        .events_published = counters.events_published,
        .handler_errors = counters.handler_errors,
        .tasks_run = counters.tasks_run,
        .task_errors = counters.task_errors,
        .last_error = (rh_u16)(counters.last_error != (rh_u16)ROBOHUB_OK
            ? counters.last_error : sensor_error),
        .probe_ok = probe_ok,
        .probe_fault = probe_fault,
    };

    rh_u8 bytes[DIAG_RESPONSE_WIRE_SIZE];
    if (DiagResponse_serialize(&response, bytes, sizeof(bytes)) == 0u) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    return robohub_core_event_publish_with_tx(MESSAGE_ID_DIAG_RESPONSE,
                                              robohub_core_event_current_tx(),
                                              bytes, (rh_size)DIAG_RESPONSE_WIRE_SIZE);
}

/* Called every pass. Reads whatever the link has delivered and feeds the
 * transport one byte at a time, so a command from the gateway becomes an event
 * exactly as a frame from a node does. */
static void pump_link(rh_u32 now_ms)
{
    (void)now_ms;
    if (link_is_net) {
        return;
    }
    if (node_uart != 0 && node_uart->read_available != 0) {
        rh_u8 buffer[64];
        rh_size got = node_uart->read_available(node_uart, buffer, sizeof(buffer));
        for (rh_size i = 0; i < got; i++) {
            robohub_transport_feed_byte(&node_link, buffer[i]);
        }
        return;
    }
    for (int i = 0; i < 64; i++) {
        int byte = getchar_timeout_us(0);
        if (byte == PICO_ERROR_TIMEOUT) {
            break;
        }
        robohub_transport_feed_byte(&node_link, (rh_u8)byte);
    }
}

static const robohub_core_app sensor_app = {
    .name = "sensor",
    .init = sensor_init,
    .start = sensor_start,
    .step = pump_link,
};

/* -------------------------------------------------------------------------- */
/*  The second link: Wi-Fi, framed packets over TCP                           */
/* -------------------------------------------------------------------------- */

#if defined(CONFIG_RH_NET) && CONFIG_RH_NET

/* Bring the interface up and open a TCP socket to the gateway. Best effort: a
 * board that cannot reach one still streams over USB, and says so. */
static void net_sink_start(void)
{
    if (RH_WIFI_SSID[0] == '\0') {
        emit("wifi: no credentials configured; streaming over USB only\n");
        return;
    }

    RoboHubWiFiConfig wifi = {
        .ssid = RH_WIFI_SSID,
        .password = RH_WIFI_PASSWORD,
        .timeout_ms = 15000u,
    };
    if (robohub_net_wifi_connect(&wifi) != ROBOHUB_OK) {
        emit("wifi: could not join; streaming over USB only\n");
        return;
    }

    char ip[16];
    if (robohub_net_ip(ip, sizeof(ip)) == ROBOHUB_OK) {
        emit("wifi: joined, address ");
        emit(ip);
        emit("\n");
    }

    RoboHubSocket socket = robohub_socket_create(ROBOHUB_SOCKET_TCP);
    if (socket == 0) {
        emit("net: no socket available; streaming over USB only\n");
        return;
    }
    if (robohub_socket_connect(socket, RH_TELEMETRY_HOST, RH_TELEMETRY_PORT) != ROBOHUB_OK) {
        robohub_socket_close(socket);
        emit("net: gateway unreachable; streaming over USB only\n");
        return;
    }

    RoboHubTelemetrySink net_sink = {
        .type = ROBOHUB_SINK_SOCKET,
        .handle.socket = socket,
        .enabled = RH_TRUE,
    };
    if (robohub_telemetry_add_sink(&net_sink) != ROBOHUB_OK) {
        robohub_socket_close(socket);
        emit("net: no room for the socket sink; streaming over USB only\n");
        return;
    }
    emit("net: telemetry also streams over TCP\n");
}

#endif /* CONFIG_RH_NET */

/* -------------------------------------------------------------------------- */
/*  Entry                                                                     */
/* -------------------------------------------------------------------------- */

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        for (;;) {
        }
    }

    /* Telemetry first, so the bridge is in place before anything publishes. */
    (void)robohub_telemetry_init();

    link_is_net = (strcmp(RH_NODE_TRANSPORT, "wifi") == 0) ||
                  (strcmp(RH_NODE_TRANSPORT, "socket") == 0);
    RoboHubTransportPhy link = { .write = usb_phy_write, .context = 0 };
    if (strcmp(RH_NODE_TRANSPORT, "uart") == 0) {
        node_uart = robohub_get_uart(RH_NODE_UART);
        if (node_uart != 0 && node_uart->init != 0 &&
            ROBOHUB_SUCCEEDED(node_uart->init(node_uart, RH_NODE_BAUD))) {
            link.write = uart_phy_write;
        } else {
            node_uart = 0;
            emit("uart: no link; using USB CDC\n");
        }
    }

    if (!link_is_net) {
        (void)robohub_transport_init(&node_link, &link);
        RoboHubTelemetrySink sink = {
            .type = ROBOHUB_SINK_TRANSPORT,
            .handle.transport = &node_link,
            .enabled = RH_TRUE,
        };
        (void)robohub_telemetry_add_sink(&sink);
    }

#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    /* WiFi and socket are their own outgoing link; the console stays text. */
    if (link_is_net) {
        net_sink_start();
    }
#endif

    (void)robohub_core_app_register(&sensor_app);
    if (ROBOHUB_FAILED(robohub_core_init(0))) {
        emit("runtime failed to start; is the sensor on the bus?\n");
        for (;;) {
            if (system_timer != 0) {
                system_timer->delay_ms(system_timer, 1000u);
            }
        }
    }

    /* Subscribe after the runtime is up: bringing it up initialises the event
     * bus, which starts with no subscriptions and would drop this one. */
    (void)robohub_telemetry_enable_event(EVENT_TEMP_READING, 1u);
    /* The acknowledgement goes back out the same link, with the caller's tx. */
    (void)robohub_telemetry_enable_event(MESSAGE_ID_CONTROL_RESPONSE, 1u);
    (void)robohub_telemetry_enable_event(MESSAGE_ID_DIAG_RESPONSE, 1u);
    /* And a command from the gateway arrives on it. */
    (void)robohub_core_event_subscribe(MESSAGE_ID_CONTROL_REQUEST, on_control, 0);
    (void)robohub_core_event_subscribe(MESSAGE_ID_DIAG_REQUEST, on_diag, 0);

    (void)robohub_core_run();
    for (;;) {
    }
}
