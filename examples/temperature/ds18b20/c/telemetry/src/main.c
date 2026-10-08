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
#include "robohub/hal/onewire.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/temperature/ds18b20.h"
#include "robohub/core/core.h"
#include "robohub/core/app.h"
#include "robohub/core/event.h"
#include "robohub/core/events.h"
#include "robohub/core/scheduler.h"
#include "robohub/transport.h"
#include "robohub/telemetry.h"
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
#define RH_WIFI_SSID ""
#endif
#ifndef RH_WIFI_PASSWORD
#define RH_WIFI_PASSWORD ""
#endif
#ifndef RH_TELEMETRY_HOST
#define RH_TELEMETRY_HOST "192.168.1.10"
#endif
#ifndef RH_TELEMETRY_PORT
#define RH_TELEMETRY_PORT 5000
#endif
#endif

/* The event the sensor layer owns. A rho schema would name it and give the
 * payload its layout; here it is written out, because the point is the way it
 * travels. */
#define EVENT_TEMP_READING RH_EVENT_ID(RH_EVENT_DOMAIN_SENSOR, 1)

typedef struct {
    rh_f32 celsius;
} temperature_reading;

static void emit(const char* text)
{
    (void)robohub_console_puts(text);
}

/* -------------------------------------------------------------------------- */
/*  The link: USB CDC, raw bytes                                              */
/* -------------------------------------------------------------------------- */

static RoboHubTransport usb_link;

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

/* -------------------------------------------------------------------------- */
/*  Sensor application                                                        */
/* -------------------------------------------------------------------------- */

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

    const RoboHubTransportPhy link = { .write = usb_phy_write, .context = 0 };
    (void)robohub_transport_init(&usb_link, &link);

    RoboHubTelemetrySink sink = {
        .type = ROBOHUB_SINK_TRANSPORT,
        .handle.transport = &usb_link,
        .enabled = RH_TRUE,
    };
    (void)robohub_telemetry_add_sink(&sink);

#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    /* The same readings, a second way out. Failure here is not fatal. */
    net_sink_start();
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

    (void)robohub_core_run();
    for (;;) {
    }
}
