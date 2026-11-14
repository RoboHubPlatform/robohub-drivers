/**
 * @file main.c
 * @brief A relay on a host: it carries boards' frames to the gateway above, unchanged.
 *
 * A relay is the headless hop of a tree: boards wire to its `uart1`, and it
 * carries them to the gateway above — over the uplink a host dials, or the USB
 * CDC a board has. This is that relay on a host, so the tree runs and is tested
 * on one machine.
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
#include "protocol.h"
#include "uplink.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* This relay's id and key, written into its build by Mesh when it planned the
 * relay and tracked by no repository. A host without them reads the same from
 * RH_SECURE_UID and RH_SECURE_KEY_FILE, as every host node does. Without either
 * the relay's links are plain, and a parent that requires authentication — every
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

/* What the relay says goes to stderr, not to the console: the console is a
 * frame link (its parent's serial link, when it has no socket to one), and a
 * line of text written between two frames would be read as part of the next. */
static void emit(const char* text)
{
    (void)fputs(text, stderr);
    (void)fflush(stderr);
}

/* -------------------------------------------------------------------------- */
/*  Identity                                                                  */
/* -------------------------------------------------------------------------- */

static rh_u8 relay_uid[RH_SECURE_UID_SIZE];
static rh_u8 relay_key[RH_SECURE_KEY_SIZE];
/* The epoch of relay_key, and the key scope a rotated key is stored with: the
 * build's (robohub_secret.h), or for a key file its epoch and no scope. */
static rh_u32 relay_epoch;
static const rh_u8* relay_scope;
static rh_bool provisioned;
static RoboHubTimer* system_timer;

static rh_u32 relay_now_ms(void)
{
    return system_timer != 0 ? (rh_u32)system_timer->get_millis(system_timer) : 0u;
}

#if !defined(RH_SECURE_PROVISIONED)
static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* `text` as exactly `size` bytes of hex, trailing whitespace allowed. */
static rh_bool hex_bytes(const char* text, rh_u8* out, rh_size size)
{
    if (text == 0) {
        return RH_FALSE;
    }
    for (rh_size i = 0; i < size; i++) {
        int hi = hex_value(text[2u * i]);
        int lo = (hi < 0) ? -1 : hex_value(text[2u * i + 1u]);
        if (hi < 0 || lo < 0) {
            return RH_FALSE;
        }
        out[i] = (rh_u8)((hi << 4) | lo);
    }
    const char* rest = text + 2u * size;
    while (*rest == '\n' || *rest == '\r' || *rest == ' ') {
        rest++;
    }
    return (*rest == '\0') ? RH_TRUE : RH_FALSE;
}
#endif

/* The build's id and key, else the environment's. A key named but unreadable
 * stops the relay rather than letting it run plain: it answers -1. */
static int load_identity(void)
{
#if defined(RH_SECURE_PROVISIONED)
    memcpy(relay_uid, RH_DEVICE_UID, sizeof(relay_uid));
    memcpy(relay_key, RH_DEVICE_KEY, sizeof(relay_key));
#  if defined(RH_DEVICE_KEY_EPOCH)
    relay_epoch = (rh_u32)RH_DEVICE_KEY_EPOCH;
#  endif
#  if defined(RH_HAS_KEY_SCOPE)
    relay_scope = RH_KEY_SCOPE;
#  endif
    provisioned = RH_TRUE;
    return 0;
#else
    const char* uid_text = getenv("RH_SECURE_UID");
    const char* key_path = getenv("RH_SECURE_KEY_FILE");
    if ((uid_text == 0 || uid_text[0] == '\0') && (key_path == 0 || key_path[0] == '\0')) {
        return 0;
    }
    char key_text[80] = { 0 };
    FILE* file = (key_path != 0 && key_path[0] != '\0') ? fopen(key_path, "r") : NULL;
    if (file != NULL) {
        size_t got = fread(key_text, 1u, sizeof(key_text) - 1u, file);
        key_text[got] = '\0';
        fclose(file);
    }
    /* A key file is `<epoch>:<hex>`, or the hex alone (epoch 0). */
    const char* key_hex = key_text;
    const char* colon = strchr(key_text, ':');
    rh_bool ok = RH_TRUE;
    if (colon != NULL) {
        relay_epoch = 0u;
        for (const char* p = key_text; p < colon && ok; p++) {
            ok = (*p >= '0' && *p <= '9' && relay_epoch <= 400000000u) ? RH_TRUE : RH_FALSE;
            relay_epoch = relay_epoch * 10u + (rh_u32)(*p - '0');
        }
        ok = ok && colon != key_text;
        key_hex = colon + 1;
    }
    ok = ok && hex_bytes(uid_text, relay_uid, sizeof(relay_uid)) && hex_bytes(key_hex, relay_key, sizeof(relay_key));
    memset(key_text, 0, sizeof(key_text));
    if (!ok) {
        emit("relay: RH_SECURE_UID or RH_SECURE_KEY_FILE is not a relay id and key\n");
        return -1;
    }
    provisioned = RH_TRUE;
    return 0;
#endif
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
/* RHO_PARENT_LINK=console: the parent is on the console — this relay is wired
 * to a gateway's serial link — and no socket is dialled. */
static rh_bool console_parent;
static RoboHubTransport uplink_link;
static RoboHubSecure uplink_secure;
static RoboHubRelayUplink relay_uplink;
static RoboHubSocket parent_socket;
static rh_u32 lost_at_ms;
static rh_bool ever_dialled;
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
    parent_socket = robohub_socket_create(ROBOHUB_SOCKET_TCP);
    if (parent_socket == 0) {
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
        return;
    }

    const RoboHubTransportPhy phy = { .write = uplink_write, .context = 0 };
    (void)robohub_transport_init(&uplink_link, &phy);
    (void)robohub_transport_on_frame(&uplink_link, on_parent_frame, &uplink_link);
    if (provisioned &&
        robohub_secure_init_epoch(&uplink_secure, relay_uid, relay_key, relay_epoch, relay_scope,
                                  robohub_platform_nonce, relay_now_ms) == ROBOHUB_OK) {
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
    if (console_parent) {
        return;
    }
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
        robohub_secure_init_epoch(&upstream_secure, relay_uid, relay_key, relay_epoch, relay_scope,
                                  robohub_platform_nonce, relay_now_ms) == ROBOHUB_OK) {
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
        for (int i = 0; i < 64; i++) {
            rh_u8 byte = 0;
            if (down_link->read(down_link, &byte, 1u) != ROBOHUB_OK) {
                break;
            }
            robohub_transport_feed_byte(&downstream, byte);
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
    /* A parent that goes away must not take the relay with it. */
    (void)signal(SIGPIPE, SIG_IGN);
    /* The console is polled, never waited on. */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }
    if (load_identity() != 0) {
        return 1;
    }
    if (!provisioned) {
        emit("relay: no id and key; its links are plain and a parent hears nothing from it\n");
    }
#if defined(CONFIG_RH_NET) && CONFIG_RH_NET
    const char* parent_link_env = getenv("RHO_PARENT_LINK");
    console_parent = (parent_link_env != 0 && strcmp(parent_link_env, "console") == 0) ? RH_TRUE : RH_FALSE;
    if (console_parent) {
        emit("relay: the parent is on the console; no uplink is dialled\n");
    }
#endif

    const RoboHubTransportPhy up_phy = { .write = upstream_write, .context = 0 };
    (void)robohub_transport_init(&upstream, &up_phy);
    (void)robohub_transport_on_frame(&upstream, on_parent_frame, &upstream);
    const RoboHubTransportPhy down_phy = { .write = downstream_write, .context = 0 };
    (void)robohub_transport_init(&downstream, &down_phy);
    (void)robohub_transport_on_frame(&downstream, on_board_frame, 0);

    (void)robohub_core_app_register(&relay_app);
    if (ROBOHUB_FAILED(robohub_core_init(0))) {
        emit("relay failed to start; is the downstream link wired?\n");
        return 1;
    }

    (void)robohub_core_run();
    return 0;
}
