/**
 * @file main.c
 * @brief A Pico that gateways without measuring: it carries boards' frames up, unchanged.
 *
 * A relay is the headless hop of a tree: boards wire to its `uart1`, and it
 * carries them to the gateway above — over the Wi-Fi uplink to its parent, or
 * the USB CDC when a host gateway is on it. `relay-host` is the same program on
 * a host, which is how the tree is run and tested on one machine.
 *
 * It holds no key of any board's. What a board sends — its HELLO, and every frame
 * it seals under its own session — goes up unchanged inside a RELAYED message,
 * sealed under the relay's own session with its parent; what the parent sends a
 * board comes down the same way, and the relay writes the inner frame to the
 * board's port. The parent checks the board's frames with the board's own key,
 * so the relay can drop or delay a reading and cannot forge or change one. A
 * board's plain frame is not carried: the parent refuses it, and the relay
 * cannot vouch for it.
 *
 * The relay acts on nothing it is sent but RELAYED and a diagnostic about itself
 * (node 0), and only from a parent link it opened a session on. The console (a
 * board's USB CDC) is such a link only when the relay is provisioned: unsealed,
 * a frame that comes in on it is dropped, so nothing on the USB port reaches a
 * board.
 *
 * Built for the board with: rho build -target pico2w .
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
#include "robohub/hal/timer.h"
#include "robohub/hal/uart.h"
#include "pico_uart.h"
#include "robohub/core/core.h"
#include "robohub/core/app.h"
#include "robohub/core/event.h"
#include "robohub/core/scheduler.h"
#include "robohub/core/diagnostics.h"
#include "robohub/transport.h"
#include "protocol.h"
#include "uplink.h"

#include <string.h>

/* This relay's id and key, written into its build by Mesh when it planned the
 * relay and tracked by no repository. Without them the relay's links are plain, and a parent that requires authentication — every
 * parent's uplink does — hears nothing from it. */
#if defined(__has_include)
#  if __has_include("robohub_secret.h")
#    include "robohub_secret.h"
#  endif
#endif
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
#include "robohub/net.h"
#endif

/* The names the board support package registers. */
#define TIMER_NAME   "system_timer"
#define DOWNSTREAM_UART "uart1"
#define DOWNSTREAM_BAUD 115200u
/* The port a board on uart1 hangs from, as RELAYED names it. */
#define DOWNSTREAM_PORT 1u

/* The gateway above this one. */
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
#ifndef RH_PARENT_HOST
#define RH_PARENT_HOST "192.168.1.10"
#endif
#ifndef RH_PARENT_PORT
#define RH_PARENT_PORT 5001
#endif
/* How long a lost parent is left before it is dialled again. */
#define REDIAL_MS 2000u
#endif

/* The area this relay speaks for: the area the boards behind it are numbered
 * in. Its parent numbers this relay by the link it arrives on, not by a node id,
 * so a relay has no node id of its own. */
#if defined(CONFIG_RH_AREA_ID)
#define RH_AREA_ID ((rh_u16)CONFIG_RH_AREA_ID)
#else
#define RH_AREA_ID ((rh_u16)1u)
#endif

#ifndef CONFIG_RH_LINK_ID
#define CONFIG_RH_LINK_ID 1
#endif

static void emit(const char* text)
{
    (void)robohub_console_puts(text);
}

/* -------------------------------------------------------------------------- */
/*  Identity                                                                  */
/* -------------------------------------------------------------------------- */

static rh_u8 relay_uid[RH_SECURE_UID_SIZE];
static rh_u8 relay_key[RH_SECURE_KEY_SIZE];
static rh_bool provisioned;
static RoboHubTimer* system_timer;

static rh_u32 relay_now_ms(void)
{
    return system_timer != 0 ? (rh_u32)system_timer->get_millis(system_timer) : 0u;
}

/* The build's id and key: a board has no other place to read them from. */
static int load_identity(void)
{
#if defined(RH_SECURE_PROVISIONED)
    memcpy(relay_uid, RH_DEVICE_UID, sizeof(relay_uid));
    memcpy(relay_key, RH_DEVICE_KEY, sizeof(relay_key));
    provisioned = RH_TRUE;
#endif
    return 0;
}

/* -------------------------------------------------------------------------- */
/*  The links                                                                 */
/* -------------------------------------------------------------------------- */

/* Upstream: the console (a board's USB CDC), written raw because a frame is
 * bytes, not a line. It is a parent link only when it is sealed. */
static RoboHubTransport upstream;
static RoboHubSecure upstream_secure;
static RoboHubRelayUplink console_uplink;

static int upstream_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    return (robohub_console_write_raw(data, length) == ROBOHUB_CONSOLE_OK) ? 0 : -1;
}

/* Downstream: the UART the boards are wired to. */
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

#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
static RoboHubTransport uplink_link;
static RoboHubSecure uplink_secure;
static RoboHubRelayUplink relay_uplink;
static RoboHubSocket parent_socket;
static rh_u32 lost_at_ms;
static rh_bool ever_dialled;
static rh_bool wifi_up;
/* The session the announce last went up under: a new one is a parent that may
 * have restarted and forgotten this relay, so it is told again. */
static rh_u16 announced_session;
static rh_bool announced_sealed;

static int uplink_write(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    if (parent_socket == 0) {
        return -1;
    }
    return (robohub_socket_send(parent_socket, data, length) < 0) ? -1 : 0;
}
#endif

/* The link the boards' frames go up: the socket to the parent when it is up,
 * else the console when it is sealed. NULL when there is neither. */
static RoboHubRelayUplink* parent_link(void)
{
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    if (parent_socket != 0) {
        return &relay_uplink;
    }
#endif
    if (provisioned) {
        return &console_uplink;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/*  A board's frames, up                                                      */
/* -------------------------------------------------------------------------- */

static rh_u32 board_plain_dropped;

/* Every frame a board sends is taken here, before the bus: its HELLO and its
 * sealed frames go up whole inside RELAYED, and nothing else goes anywhere. */
static int on_board_frame(rh_u16 msg_id, rh_u16 tx_id, const rh_u8* payload, rh_size length, void* user)
{
    (void)user;
    if (!robohub_relayed_carried(msg_id)) {
        board_plain_dropped++;
        return 1;
    }
    RoboHubRelayUplink* up = parent_link();
    if (up != 0) {
        (void)robohub_uplink_carry(up, (rh_u8)DOWNSTREAM_PORT, msg_id, tx_id, payload, length);
    }
    return 1;
}

/* -------------------------------------------------------------------------- */
/*  The parent's frames, down                                                 */
/* -------------------------------------------------------------------------- */

/* A diagnostic about this relay itself — node 0 of its area — answered on the
 * link it came on. A diagnostic for a board comes down inside RELAYED, sealed
 * for the board, like everything else for it. */
static void answer_diag(RoboHubTransport* link, rh_u16 tx_id, const rh_u8* payload, rh_size size)
{
    DiagRequest request;
    if (!DiagRequest_deserialize((const uint8_t*)payload, (size_t)size, &request) || request.device.node_id != 0) {
        return;
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
    if (DiagResponse_serialize(&response, bytes, sizeof(bytes)) != 0u) {
        (void)robohub_transport_send(link, MESSAGE_ID_DIAG_RESPONSE, tx_id, bytes, (rh_u16)DIAG_RESPONSE_WIRE_SIZE);
    }
}

/* What a parent link delivers. On a sealed link the transport has already
 * dropped whatever did not open under the relay's session, so this is the
 * parent speaking; an unsealed link is nobody in particular, and nothing it
 * says is acted on. `user` is the link. */
static int on_parent_frame(rh_u16 msg_id, rh_u16 tx_id, const rh_u8* payload, rh_size length, void* user)
{
    RoboHubTransport* link = (RoboHubTransport*)user;
    if (link == 0 || link->secure == 0) {
        return 1;
    }
    if (msg_id == MESSAGE_ID_RELAYED) {
        RoboHubRelayed relayed;
        if (robohub_relayed_unwrap(payload, length, &relayed) == ROBOHUB_OK &&
            relayed.port == (rh_u8)DOWNSTREAM_PORT && down_link != 0 && relayed.length <= 0xFFFFu) {
            /* The downstream transport is plain, so the board's frame is written
             * exactly as the parent framed it: its SESSION, or a frame sealed
             * for it. */
            (void)robohub_transport_send(&downstream, (robohub_event_id)relayed.msg_id, relayed.tx_id,
                                         relayed.payload, (rh_u16)relayed.length);
        }
        return 1;
    }
    if (msg_id == MESSAGE_ID_DIAG_REQUEST) {
        answer_diag(link, tx_id, payload, length);
    }
    return 1;
}

/* -------------------------------------------------------------------------- */
/*  The uplink: the gateway above, over a socket                              */
/* -------------------------------------------------------------------------- */

#if defined(CONFIG_RH_NET) && CONFIG_RH_NET

static void uplink_lost(const char* why)
{
    if (parent_socket != 0) {
        robohub_socket_close(parent_socket);
        parent_socket = 0;
    }
    lost_at_ms = relay_now_ms();
    announced_sealed = RH_FALSE;
    emit(why);
}

/* Dial the parent and set the link up: a new transport, and a new session to be
 * asked for, since a parent that went away has forgotten the old one. */
static void uplink_dial(void)
{
    ever_dialled = RH_TRUE;
    lost_at_ms = relay_now_ms();
    if (!wifi_up) {
        RoboHubWiFiConfig wifi = { .ssid = "", .password = "", .timeout_ms = 15000u };
        if (robohub_net_wifi_connect(&wifi) != ROBOHUB_OK) {
            return;
        }
        wifi_up = RH_TRUE;
    }
    parent_socket = robohub_socket_create(ROBOHUB_SOCKET_TCP);
    if (parent_socket == 0) {
        return;
    }
    if (robohub_socket_connect(parent_socket, RH_PARENT_HOST, (rh_u16)RH_PARENT_PORT) != ROBOHUB_OK) {
        robohub_socket_close(parent_socket);
        parent_socket = 0;
        return;
    }

    const RoboHubTransportPhy phy = { .write = uplink_write, .context = 0 };
    (void)robohub_transport_init(&uplink_link, &phy);
    (void)robohub_transport_on_frame(&uplink_link, on_parent_frame, &uplink_link);
    if (provisioned &&
        robohub_secure_init(&uplink_secure, relay_uid, relay_key, robohub_platform_nonce, relay_now_ms) == ROBOHUB_OK) {
        (void)robohub_transport_secure(&uplink_link, &uplink_secure);
    }

    relay_uplink.link = &uplink_link;
    relay_uplink.gateway_area = RH_AREA_ID;
    relay_uplink.link_id = (rh_u16)CONFIG_RH_LINK_ID;
    relay_uplink.area = RH_AREA_ID;
    relay_uplink.announced = RH_FALSE;
    announced_sealed = RH_FALSE;

    /* On a sealed link this first send has no session to go under: it says
     * hello, and the announce goes up once the session is open. */
    robohub_uplink_announce(&relay_uplink);
    emit(provisioned ? "uplink: dialled the parent; asking for a session\n"
                     : "uplink: dialled the parent, unauthenticated: it will not hear the boards\n");
}

/* Move whatever the parent has sent into the uplink transport; dial again when
 * the parent went away; and announce whenever the session is new. */
static void uplink_pump(void)
{
    if (parent_socket == 0) {
        if (!ever_dialled || (rh_u32)(relay_now_ms() - lost_at_ms) >= REDIAL_MS) {
            uplink_dial();
        }
        return;
    }
    rh_u8 buf[256];
    int got = robohub_socket_recv(parent_socket, buf, (rh_size)sizeof(buf), 0u);
    if (got > 0) {
        robohub_transport_feed(&uplink_link, buf, (rh_size)got);
    } else if (got == 0 || got == ROBOHUB_SOCKET_ERROR) {
        uplink_lost("uplink: the parent went away; dialling again\n");
        return;
    }
    if (!provisioned) {
        return;
    }
    if (robohub_secure_ready(&uplink_secure)) {
        if (!announced_sealed || uplink_secure.session_id != announced_session) {
            relay_uplink.announced = RH_FALSE;
            robohub_uplink_announce(&relay_uplink);
            if (relay_uplink.announced) {
                announced_sealed = RH_TRUE;
                announced_session = uplink_secure.session_id;
                emit("uplink: announced under the session\n");
            }
        }
    } else if (announced_sealed) {
        /* The session ran out: this send asks for another, and the announce
         * follows it once it is open. */
        announced_sealed = RH_FALSE;
        robohub_uplink_announce(&relay_uplink);
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
    if (provisioned &&
        robohub_secure_init(&upstream_secure, relay_uid, relay_key, robohub_platform_nonce, relay_now_ms) == ROBOHUB_OK) {
        (void)robohub_transport_secure(&upstream, &upstream_secure);
    }
    console_uplink.link = &upstream;
    console_uplink.gateway_area = RH_AREA_ID;
    console_uplink.link_id = (rh_u16)CONFIG_RH_LINK_ID;
    console_uplink.area = RH_AREA_ID;
    console_uplink.announced = RH_FALSE;
    return ROBOHUB_OK;
}

/* Every pass: move bytes from each link into its transport. A board's frame is
 * carried up as it completes; a parent's is acted on as it opens. */
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

    /* The console is read either way, so whatever writes to it never blocks;
     * an unsealed console's frames are dropped by its hook. */
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
    (void)load_identity();
    if (!provisioned) {
        emit("relay: no id and key; its links are plain and a parent hears nothing from it\n");
    }

    const RoboHubTransportPhy up_phy = { .write = upstream_write, .context = 0 };
    (void)robohub_transport_init(&upstream, &up_phy);
    (void)robohub_transport_on_frame(&upstream, on_parent_frame, &upstream);
    const RoboHubTransportPhy down_phy = { .write = downstream_write, .context = 0 };
    (void)robohub_transport_init(&downstream, &down_phy);
    (void)robohub_transport_on_frame(&downstream, on_board_frame, 0);

    (void)robohub_core_app_register(&relay_app);
    if (ROBOHUB_FAILED(robohub_core_init(0))) {
        emit("relay failed to start; is the downstream link wired?\n");
        for (;;) {
            if (system_timer != 0) {
                system_timer->delay_ms(system_timer, 1000u);
            }
        }
    }

    (void)robohub_core_run();
    for (;;) {
    }
}
