/**
 * @file uplink.c
 * @brief The relay's uplink: announce, and carry boards' frames up in RELAYED.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "uplink.h"

#include <string.h>

// @req DRV-GW-0003
void robohub_uplink_announce(RoboHubRelayUplink* uplink)
{
    if (uplink == 0 || uplink->link == 0) {
        return;
    }

    Announce announce;
    memset(&announce, 0, sizeof(announce));
    announce.gateway.area_id = uplink->gateway_area;
    announce.gateway.link_id = uplink->link_id;
    announce.firmware_version = 1u;
    announce.mode = MODE_RELAY;
    announce.capabilities = CAPABILITY_GATEWAY | CAPABILITY_DIAGNOSTICS | CAPABILITY_UPLINK;
    announce.area_id = uplink->area;

    rh_u8 bytes[ANNOUNCE_WIRE_SIZE];
    if (Announce_serialize(&announce, bytes, sizeof(bytes)) == 0u) {
        return;
    }
    if (robohub_transport_send(uplink->link, MESSAGE_ID_ANNOUNCE, 0u, bytes, (rh_u16)ANNOUNCE_WIRE_SIZE) == ROBOHUB_OK) {
        uplink->announced = RH_TRUE;
    }
}

// @req DRV-GW-0001
rh_bool robohub_relayed_carried(rh_u16 msg_id)
{
    return (msg_id == RH_SECURE_MSG_HELLO || (msg_id & RH_SECURE_FLAG) != 0u) ? RH_TRUE : RH_FALSE;
}

rh_size robohub_relayed_wrap(rh_u8 port, rh_u16 msg_id, rh_u16 tx_id,
                             const rh_u8* payload, rh_size length,
                             rh_u8* out, rh_size capacity)
{
    if (out == 0 || (length > 0u && payload == 0) || capacity < RH_RELAYED_HEADER_SIZE ||
        length > capacity - RH_RELAYED_HEADER_SIZE) {
        return 0u;
    }
    out[0] = port;
    out[1] = (rh_u8)(msg_id & 0xFFu);
    out[2] = (rh_u8)(msg_id >> 8);
    out[3] = (rh_u8)(tx_id & 0xFFu);
    out[4] = (rh_u8)(tx_id >> 8);
    if (length > 0u) {
        memcpy(out + RH_RELAYED_HEADER_SIZE, payload, length);
    }
    return RH_RELAYED_HEADER_SIZE + length;
}

RoboHubError robohub_relayed_unwrap(const rh_u8* payload, rh_size length, RoboHubRelayed* out)
{
    if (payload == 0 || out == 0 || length < RH_RELAYED_HEADER_SIZE) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    out->port = payload[0];
    out->msg_id = (rh_u16)(payload[1] | ((rh_u16)payload[2] << 8));
    out->tx_id = (rh_u16)(payload[3] | ((rh_u16)payload[4] << 8));
    out->payload = payload + RH_RELAYED_HEADER_SIZE;
    out->length = length - RH_RELAYED_HEADER_SIZE;
    return ROBOHUB_OK;
}

RoboHubError robohub_uplink_carry(RoboHubRelayUplink* uplink, rh_u8 port, rh_u16 msg_id, rh_u16 tx_id,
                                  const rh_u8* payload, rh_size length)
{
    if (uplink == 0 || uplink->link == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rh_u8 relayed[RH_TRANSPORT_MAX_FRAME_SIZE];
    rh_size size = robohub_relayed_wrap(port, msg_id, tx_id, payload, length, relayed, sizeof(relayed));
    if (size == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return robohub_transport_send(uplink->link, MESSAGE_ID_RELAYED, 0u, relayed, (rh_u16)size);
}
