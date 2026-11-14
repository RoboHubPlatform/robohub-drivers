/**
 * @file uplink.h
 * @brief A relay's link to the gateway above it.
 *
 * The relay hears boards on its downstream link and carries them up. This is the
 * other half: the announce that says which area it speaks for, and the RELAYED
 * envelope a board's frame travels in.
 *
 * A relay holds no key of a board's. A board's HELLO and every frame it seals
 * under its own session go up unchanged, inside a RELAYED message the relay
 * seals under its own session with the parent:
 *
 *     port(u8) | inner_msg_id(u16) | inner_tx_id(u16) | inner_payload
 *
 * The parent authenticates the board with the board's own key and answers it
 * the same way: the board's SESSION and every frame it seals for the board come
 * down inside RELAYED, and the relay writes the inner frame to the port. So a
 * relay can drop or delay a board's frame, and cannot forge or change one.
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

/** What RELAYED adds before the inner payload: the port and the inner header. */
#define RH_RELAYED_HEADER_SIZE 5u

/** A relay's link to its parent: the framing, and who this relay is. */
typedef struct {
    /** The link to the parent, as a framed transport. Owned by the caller. */
    RoboHubTransport* link;
    /** This relay's area and link id: its gateway identity to its parent. */
    rh_u16 gateway_area;
    rh_u16 link_id;
    /** The area this relay speaks for — what its downstream boards number in. */
    rh_u16 area;
    /** Whether the announce has gone out on this link. */
    rh_bool announced;
} RoboHubRelayUplink;

/** A RELAYED payload, read: the port and the board's frame, which points into the payload. */
typedef struct {
    rh_u8 port;
    rh_u16 msg_id;
    rh_u16 tx_id;
    const rh_u8* payload;
    rh_size length;
} RoboHubRelayed;

/**
 * @brief Frame an announce and send it to the parent.
 *
 * A parent builds its child table from these, so it must be sent once the link
 * is up and again whenever the session under it is new — a parent that
 * restarted has forgotten it.
 */
void robohub_uplink_announce(RoboHubRelayUplink* uplink);

/**
 * @brief Whether a board's frame is one a relay carries: its HELLO, or a frame
 * sealed under its own session. A plain frame is not — the parent would refuse
 * it, and the relay cannot vouch for it.
 */
rh_bool robohub_relayed_carried(rh_u16 msg_id);

/**
 * @brief Write the RELAYED payload for one board frame into `out`; returns its
 * length, or 0 when it does not fit.
 */
rh_size robohub_relayed_wrap(rh_u8 port, rh_u16 msg_id, rh_u16 tx_id,
                             const rh_u8* payload, rh_size length,
                             rh_u8* out, rh_size capacity);

/** Read a RELAYED payload; ROBOHUB_ERR_PROTOCOL when it is too short to be one. */
RoboHubError robohub_relayed_unwrap(const rh_u8* payload, rh_size length, RoboHubRelayed* out);

/**
 * @brief Carry one board frame up, unchanged, inside RELAYED on the parent link
 * (sealed there when the link is authenticated).
 */
RoboHubError robohub_uplink_carry(RoboHubRelayUplink* uplink, rh_u8 port, rh_u16 msg_id, rh_u16 tx_id,
                                  const rh_u8* payload, rh_size length);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_UPLINK_H */
