/**
 * @file main.c
 * @brief A relay's uplink, on a host: dial the master, announce, send a reading.
 *
 * The gateway-only mode of a relay, built for `robohub-linux` rather than a
 * board. It is the smallest program that exercises the uplink end to end: bring
 * the platform up, connect to the gateway above, say which area it speaks for,
 * and send one reading up.
 *
 * A real relay hears its readings on a downstream link; this one is handed a
 * reading directly, because what it demonstrates is the uplink, not the sensor.
 *
 *     rho build -target native .
 *     ./build/output/uplink-host <host> <port>
 *
 * With RH_UPLINK_UID (16 hex digits) and RH_UPLINK_KEY (32) in the environment —
 * the relay's id and the key its parent derives for it — the uplink is
 * authenticated: it opens a session first, and announces and reports under it.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "platform.h"
#include "robohub/net.h"
#include "robohub/transport.h"
#include "protocol.h"
#include "uplink.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static RoboHubSocket parent;

static int write_socket(const rh_u8* data, rh_size length, void* context)
{
    (void)context;
    return (robohub_socket_send(parent, data, length) < 0) ? -1 : 0;
}

static rh_u32 now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (rh_u32)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

/* hex reads `count` bytes of hex from text, and reports whether it was that. */
static int hex(const char* text, rh_u8* out, size_t count)
{
    if (text == 0 || strlen(text) != count * 2) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        unsigned value;
        if (sscanf(text + 2 * i, "%2x", &value) != 1) {
            return 0;
        }
        out[i] = (rh_u8)value;
    }
    return 1;
}

/* pump feeds whatever the parent sent into the link, for up to `ms`, and stops
 * early once `ready` says the session is open. */
static void pump(RoboHubTransport* link, RoboHubSecure* secure, rh_u32 ms)
{
    rh_u32 start = now_ms();
    while (now_ms() - start < ms) {
        rh_u8 buffer[128];
        int got = robohub_socket_recv(parent, buffer, (rh_size)sizeof(buffer), 20u);
        if (got > 0) {
            robohub_transport_feed(link, buffer, (rh_size)got);
        }
        if (secure != 0 && robohub_secure_ready(secure)) {
            return;
        }
    }
}

/* A non-blocking connect finishes in the background; wait for it rather than
 * sending into a socket that is not open yet. */
static void wait_writable(int fd)
{
    for (int attempt = 0; attempt < 200; attempt++) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        struct timeval tv = { 0, 10000 };
        if (select(fd + 1, 0, &writable, 0, &tv) > 0) {
            return;
        }
    }
}

int main(int argc, char** argv)
{
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    rh_u16 port = (argc > 2) ? (rh_u16)atoi(argv[2]) : 5001;

    if (robohub_platform_init() != ROBOHUB_OK) {
        fprintf(stderr, "the platform did not come up\n");
        return 1;
    }

    parent = robohub_socket_create(ROBOHUB_SOCKET_TCP);
    if (parent == 0) {
        fprintf(stderr, "no socket\n");
        return 1;
    }
    if (robohub_socket_connect(parent, host, port) != ROBOHUB_OK) {
        fprintf(stderr, "could not reach the gateway at %s:%u\n", host, (unsigned)port);
        return 1;
    }
    wait_writable((int)(intptr_t)parent);

    RoboHubTransport link;
    const RoboHubTransportPhy phy = { .write = write_socket, .context = 0 };
    if (robohub_transport_init(&link, &phy) != ROBOHUB_OK) {
        fprintf(stderr, "the link did not come up\n");
        return 1;
    }

    /* This program's identity: the area it speaks for, and its link id. A real
     * deployment reads these from its configuration. */
    RoboHubRelayUplink uplink = {
        .link = &link,
        .gateway_area = 7,
        .link_id = 3,
        .area = 9,
        .announced = RH_FALSE,
    };

    static RoboHubSecure secure;
    rh_u8 uid[RH_SECURE_UID_SIZE], key[RH_SECURE_KEY_SIZE];
    int authenticated = hex(getenv("RH_UPLINK_UID"), uid, sizeof(uid)) &&
                        hex(getenv("RH_UPLINK_KEY"), key, sizeof(key));
    if (authenticated) {
        if (robohub_secure_init(&secure, uid, key, robohub_platform_nonce, now_ms) != ROBOHUB_OK ||
            robohub_transport_secure(&link, &secure) != ROBOHUB_OK) {
            fprintf(stderr, "the uplink could not be authenticated\n");
            return 1;
        }
        /* The first send has no session to go under; it says hello instead. */
        robohub_uplink_announce(&uplink);
        pump(&link, &secure, 3000u);
        if (!robohub_secure_ready(&secure)) {
            fprintf(stderr, "the parent opened no session\n");
            return 1;
        }
        printf("uplink: session open\n");
    }
    robohub_uplink_announce(&uplink);

    Telemetry reading;
    memset(&reading, 0, sizeof(reading));
    reading.device.area_id = 9;
    reading.device.node_id = 6;
    reading.timestamp_ms = 5555;
    reading.temperature_c = 18.25f;
    reading.status = NODE_STATUS_OK;

    rh_u8 payload[TELEMETRY_WIRE_SIZE];
    if (Telemetry_serialize(&reading, payload, sizeof(payload)) == 0u) {
        fprintf(stderr, "the reading did not serialize\n");
        return 1;
    }
    if (robohub_uplink_send_telemetry(&uplink, payload, (rh_size)sizeof(payload)) != ROBOHUB_OK) {
        fprintf(stderr, "the reading did not send\n");
        return 1;
    }

    printf("uplink: announced area %u and sent a reading for %u/%u\n",
           (unsigned)uplink.area, (unsigned)reading.device.area_id, (unsigned)reading.device.node_id);

    usleep(200000);
    robohub_socket_close(parent);
    return 0;
}
