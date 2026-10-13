/**
 * @file uplink.h
 * @brief A relay's link to the gateway above it.
 *
 * The relay hears boards on its downstream link and streams them up. This is the
 * other half: the socket to its parent, the announce that says which area it
 * speaks for, and the re-framing of each reading it heard into the reading it
 * sends up.
 *
 * A reading is the same reading at either level — the same device, time,
 * temperature and status — but the L1 reading is the uplink's shape, so it is
 * built here from the L2 telemetry rather than forwarded as bytes. That is the
 * one place the two levels differ, and it is one function.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef RELAY_UPLINK_H
#define RELAY_UPLINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "error.h"
#include "protocol.h"
#include "robohub/transport.h"

/** A relay's link to its parent: the framing, and who this relay is. */
typedef struct {
    /** The socket, as a framed transport. Owned by the caller. */
    RoboHubTransport* link;
    /** This relay's area and link id: its gateway identity to its parent. */
    rh_u16 gateway_area;
    rh_u16 link_id;
    /** The area this relay speaks for — what its downstream boards number in. */
    rh_u16 area;
    /** Whether the announce has gone out on this link. */
    rh_bool announced;
} RoboHubRelayUplink;

/**
 * @brief Frame an announce and send it to the parent.
 *
 * A parent builds its child table from these, so it must be sent once the link
 * is up and before any reading, or the parent has nowhere to route a command
 * back to.
 */
void robohub_uplink_announce(RoboHubRelayUplink* uplink);

/**
 * @brief Re-frame one L2 telemetry reading as an L1 reading, into `out`.
 *
 * The payload is the telemetry event's, exactly as it arrived on the downstream
 * link. A payload the schema cannot read is refused rather than forwarded: a
 * gateway that passed on bytes it did not understand would be forwarding a lie.
 *
 * It is separate from the send so the conversion can be checked without a link,
 * which is the only part of the uplink that is this relay's own.
 */
RoboHubError robohub_uplink_reframe(const rh_u8* telemetry, rh_size size, rh_u8* out, rh_size capacity);

/**
 * @brief Re-frame one L2 telemetry reading as an L1 reading and send it up.
 */
RoboHubError robohub_uplink_send_telemetry(RoboHubRelayUplink* uplink, const rh_u8* payload, rh_size size);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_UPLINK_H */
