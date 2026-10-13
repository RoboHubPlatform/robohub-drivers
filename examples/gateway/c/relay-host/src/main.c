/**
 * @file main.c
 * @brief A Pico that gateways without measuring: a relay.
 *
 * The stack is the edge's, minus the sensor:
 *
 *   the downstream    frames that arrive on uart1 publish on the event bus
 *   robohub-core      carries each to its subscribers
 *   robohub-telemetry subscribed to that event, writes each to the upstream
 *   robohub-transport frames it as COBS on the USB CDC link
 *
 * Nothing forwards a board by hand. A reading names its device, so the bus needs
 * no source table: every downstream frame publishes the same `Telemetry` event,
 * and the bridge that carries one carries all. A relay measures nothing, so it
 * has no sample task and the readings a host reads are always somebody else's —
 * which is what makes it a relay rather than an edge.
 *
 * A relay is the headless hop of a tree: boards wire to its `uart1`, and it
 * carries them to the gateway above — over the uplink a host dials, or the USB
 * CDC a board has. This is that relay on a host, so the tree runs and is tested
 * on one machine.
 *
 * Built for the host with: rho build -target native .
 *
 * @project RoboHub Platform
 * @license MIT
 */


#include "platform.h"
#include "registry.h"
#include "types.h"
#include "error.h"
#include "robohub/hal/console.h"
#include "robohub/hal/timer.h"
#include "robohub/hal/uart.h"
#include "robohub/core/core.h"
#include "robohub/core/app.h"
#include "robohub/core/event.h"
#include "robohub/core/scheduler.h"
#include "robohub/core/diagnostics.h"
#include "robohub/transport.h"
#include "robohub/telemetry.h"
#include "protocol.h"
#include "uplink.h"
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
#include "robohub/net.h"
#include <stdlib.h>
#endif

/* The names the Pico's board support package registers. */
#define TIMER_NAME   "system_timer"
#define DOWNSTREAM_UART "uart1"
#define DOWNSTREAM_BAUD 115200u

/* The gateway above this one. A relay that cannot reach it still gateways to
 * whatever is on its USB CDC, so the uplink is best effort. */
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
#ifndef RH_PARENT_HOST
#define RH_PARENT_HOST "192.168.1.10"
#endif
#ifndef RH_PARENT_PORT
#define RH_PARENT_PORT 5001
#endif
#endif

/* The event a downstream reading travels as, from the schema. */
#define EVENT_TEMP_READING MESSAGE_ID_TELEMETRY

/* The area this relay speaks for: the area the boards behind it are numbered
 * in. Its parent numbers this relay by the link it arrives on, not by a node id,
 * so a relay has no node id of its own. */
#if defined(CONFIG_RH_AREA_ID)
#define RH_AREA_ID ((rh_u16)CONFIG_RH_AREA_ID)
#else
#define RH_AREA_ID ((rh_u16)1u)
#endif

static void emit(const char* text)
{
    (void)robohub_console_puts(text);
}

/* -------------------------------------------------------------------------- */
/*  The two links                                                             */
/* -------------------------------------------------------------------------- */

/* Upstream: the USB CDC, written raw because a frame is bytes, not a line. */
static RoboHubTransport upstream;

static int upstream_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    return (robohub_console_write_raw(data, length) == ROBOHUB_CONSOLE_OK) ? 0 : -1;
}

/* Downstream: the UART the boards are wired to. */
static RoboHubTransport downstream;
static RoboHubUART* down_link;
static RoboHubTimer* system_timer;

static int downstream_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    if (down_link == 0) {
        return -1;
    }
    return (down_link->write(down_link, data, length) == ROBOHUB_OK) ? 0 : -1;
}

/* -------------------------------------------------------------------------- */
/*  The uplink: the gateway above, over a socket                              */
/* -------------------------------------------------------------------------- */

#if defined(CONFIG_RH_NET) && CONFIG_RH_NET

/* This relay's identity to its parent and the area it speaks for. The link id
 * distinguishes it from another relay in the same area; a deployment that has
 * one relay can leave it at 1. */
#ifndef CONFIG_RH_LINK_ID
#define CONFIG_RH_LINK_ID 1
#endif

static RoboHubTransport uplink_link;
static RoboHubRelayUplink relay_uplink;
static RoboHubSocket parent_socket;

static int uplink_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    if (parent_socket == 0) {
        return -1;
    }
    return (robohub_socket_send(parent_socket, data, length) < 0) ? -1 : 0;
}

/* One reading, two ways out: the bus bridge already streams it over the USB CDC,
 * and this sends it up the socket as the uplink's own reading. */
static RoboHubError forward_up(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;
    if (!relay_uplink.announced) {
        return ROBOHUB_OK;
    }
    return robohub_uplink_send_telemetry(&relay_uplink, (const rh_u8*)payload, size);
}

/* Join the network and dial the parent. Best effort: a relay that cannot reach
 * its parent still gateways to a host on the USB CDC. */
static void uplink_start(void)
{
    RoboHubWiFiConfig wifi = { .ssid = "", .password = "", .timeout_ms = 15000u };
    if (robohub_net_wifi_connect(&wifi) != ROBOHUB_OK) {
        emit("uplink: no network; streaming over USB only\n");
        return;
    }

    parent_socket = robohub_socket_create(ROBOHUB_SOCKET_TCP);
    if (parent_socket == 0) {
        emit("uplink: no socket available; streaming over USB only\n");
        return;
    }
    /* A host names its parent in the environment, which a board cannot: the
     * compiled default is what a board is built with, and this is what a
     * deployment changes without a rebuild. */
    const char* parent_host = getenv("RHO_PARENT_HOST");
    if (parent_host == 0 || parent_host[0] == '\0') {
        parent_host = RH_PARENT_HOST;
    }
    rh_u32 parent_port = (rh_u32)RH_PARENT_PORT;
    const char* parent_port_env = getenv("RHO_PARENT_PORT");
    if (parent_port_env != 0 && parent_port_env[0] != '\0') {
        parent_port = (rh_u32)atoi(parent_port_env);
    }
    if (robohub_socket_connect(parent_socket, parent_host, (rh_u16)parent_port) != ROBOHUB_OK) {
        robohub_socket_close(parent_socket);
        parent_socket = 0;
        emit("uplink: parent unreachable; streaming over USB only\n");
        return;
    }

    const RoboHubTransportPhy phy = { .write = uplink_write, .context = 0 };
    (void)robohub_transport_init(&uplink_link, &phy);

    relay_uplink.link = &uplink_link;
    relay_uplink.gateway_area = RH_AREA_ID;
    relay_uplink.link_id = (rh_u16)CONFIG_RH_LINK_ID;
    relay_uplink.area = RH_AREA_ID;
    relay_uplink.announced = RH_FALSE;

    /* The parent cannot route anything down until it knows this relay exists. */
    robohub_uplink_announce(&relay_uplink);
    emit("uplink: announced to the parent\n");
}

/* Move whatever the parent has sent into the uplink transport, so a command it
 * routes down publishes on the bus exactly as one from the local link does. */
static void uplink_pump(void)
{
    if (parent_socket == 0) {
        return;
    }
    rh_u8 buf[64];
    int got = robohub_socket_recv(parent_socket, buf, (rh_size)sizeof(buf), 0u);
    if (got > 0) {
        robohub_transport_feed(&uplink_link, buf, (rh_size)got);
    }
}

#endif /* CONFIG_RH_NET */

/* -------------------------------------------------------------------------- */
/*  Application                                                               */
/* -------------------------------------------------------------------------- */

static RoboHubError relay_init(void)
{
    system_timer = robohub_get_timer(TIMER_NAME);
    down_link = robohub_get_uart(DOWNSTREAM_UART);
    if (system_timer == 0 || down_link == 0) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return down_link->init(down_link, DOWNSTREAM_BAUD);
}

static RoboHubError relay_start(void)
{
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    /* The uplink is best effort and does not block the relay: a parent that is
     * not there leaves a relay that still gateways to the USB CDC. */
    uplink_start();
#endif
    return ROBOHUB_OK;
}

/* A control from the gateway above, addressed to a board behind this relay: it
 * goes down the downstream link with the parent's transaction id intact, so the
 * board answers that transaction and the answer can find its way back. */
static RoboHubError route_down(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;
    if (down_link == 0) {
        return ROBOHUB_ERR_STATE;
    }
    return robohub_transport_send(&downstream, MESSAGE_ID_CONTROL_REQUEST,
                                  robohub_core_event_current_tx(), payload, (rh_u16)size);
}

/* The board's answer, heard on the downstream link: it goes up the uplink with
 * the same transaction id as the command, which is how the gateway that routed
 * the command matches this answer to it.
 *
 * The upstream USB CDC carries a copy too, through the telemetry bridge, so a
 * host watching the console sees the exchange; the uplink socket is what
 * actually answers the parent. */
static RoboHubError route_up(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    if (relay_uplink.announced) {
        return robohub_transport_send(&uplink_link, MESSAGE_ID_CONTROL_RESPONSE,
                                      robohub_core_event_current_tx(), payload, (rh_u16)size);
    }
#endif
    return ROBOHUB_OK;
}

/* A diagnostic the parent asks for, addressed to a board behind this relay, is
 * routed down the same way a control is. */
static RoboHubError route_diag_down(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;

    DiagRequest request;
    if (!DiagRequest_deserialize((const uint8_t*)payload, (size_t)size, &request)) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    /* Node 0 is the gateway itself, which answers above; anything else is a
     * board behind the relay, and goes down. */
    if (request.device.node_id == 0 || down_link == 0) {
        return ROBOHUB_OK;
    }
    return robohub_transport_send(&downstream, MESSAGE_ID_DIAG_REQUEST,
                                  robohub_core_event_current_tx(), payload, (rh_u16)size);
}

static RoboHubError route_diag_up(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    if (relay_uplink.announced) {
        return robohub_transport_send(&uplink_link, MESSAGE_ID_DIAG_RESPONSE,
                                      robohub_core_event_current_tx(), payload, (rh_u16)size);
    }
#endif
    return ROBOHUB_OK;
}
/* A diagnostic request: the runtime's counters and the smoke the mask asks for.
 * A relay answers for its own clock and buses — which is what tells a parent
 * whether the hop is healthy, separately from the boards behind it. */
static RoboHubError on_diag(robohub_event_id id, const void* payload, rh_size size, void* user)
{
    (void)id;
    (void)user;

    DiagRequest request;
    if (!DiagRequest_deserialize((const uint8_t*)payload, (size_t)size, &request)) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    /* A relay answers for itself — node 0 — and forwards a board's diagnostic
     * down. Answering for a board would be answering about a device this relay
     * is not. */
    if (request.device.node_id != 0) {
        return ROBOHUB_OK;
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
        .last_error = (rh_u16)counters.last_error,
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

/* Every pass: move bytes from each link into its transport. A byte completed
 * into a frame publishes on the bus, so a downstream reading takes the same path
 * to the upstream sink that an edge's own reading would.
 *
 * The downstream read is the HAL's, one byte at a time: the contract transfers
 * the bytes it is asked for and reports whether it did, so a byte is the unit
 * that carries a count without a platform-specific "how much is waiting". */
static void pump(rh_u32 now_ms)
{
    (void)now_ms;

    if (down_link != 0) {
        for (int i = 0; i < 64; i++) {
            rh_u8 byte = 0;
            if (down_link->read(down_link, &byte, 1u) != ROBOHUB_OK) {
                break;
            }
            robohub_transport_feed_byte(&downstream, byte);
        }
    }

    for (int i = 0; i < 64; i++) {
        int byte = robohub_console_read_byte();
        if (byte < 0) {
            break;
        }
        robohub_transport_feed_byte(&upstream, (rh_u8)byte);
    }

#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    uplink_pump();
#endif
}

static const robohub_core_app relay_app = {
    .name = "relay",
    .init = relay_init,
    .start = relay_start,
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

    (void)robohub_core_app_register(&relay_app);
    if (ROBOHUB_FAILED(robohub_core_init(0))) {
        emit("relay failed to start; is the downstream link wired?\n");
        for (;;) {
            if (system_timer != 0) {
                system_timer->delay_ms(system_timer, 1000u);
            }
        }
    }

    /* Every reading a downstream board sends streams up. The relay has nothing
     * of its own to publish but its diagnostics. */
    (void)robohub_telemetry_enable_event(EVENT_TEMP_READING, 1u);
    (void)robohub_telemetry_enable_event(MESSAGE_ID_DIAG_RESPONSE, 1u);
    (void)robohub_telemetry_enable_event(MESSAGE_ID_CONTROL_RESPONSE, 1u);
    (void)robohub_core_event_subscribe(MESSAGE_ID_DIAG_REQUEST, on_diag, 0);

    /* A command or a diagnostic for a board behind this relay is routed down;
     * the board's answer comes back up the same way, with the parent's
     * transaction id, so the gateway that asked matches the answer to it. */
    (void)robohub_core_event_subscribe(MESSAGE_ID_CONTROL_REQUEST, route_down, 0);
    (void)robohub_core_event_subscribe(MESSAGE_ID_CONTROL_RESPONSE, route_up, 0);
    (void)robohub_core_event_subscribe(MESSAGE_ID_DIAG_REQUEST, route_diag_down, 0);
    (void)robohub_core_event_subscribe(MESSAGE_ID_DIAG_RESPONSE, route_diag_up, 0);
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    /* Every reading also goes up to the parent, re-framed as the uplink's. */
    (void)robohub_core_event_subscribe(EVENT_TEMP_READING, forward_up, 0);
#endif

    (void)robohub_core_run();
    for (;;) {
    }
}
