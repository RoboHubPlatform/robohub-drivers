/**
 * @file uplink.c
 * @brief The relay's uplink: announce, and re-frame readings up.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "uplink.h"

#include <string.h>

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
    (void)robohub_transport_send(uplink->link, MESSAGE_ID_ANNOUNCE, 0u, bytes, (rh_u16)ANNOUNCE_WIRE_SIZE);
    uplink->announced = RH_TRUE;
}

RoboHubError robohub_uplink_reframe(const rh_u8* telemetry, rh_size size, rh_u8* out, rh_size capacity)
{
    if (telemetry == 0 || out == 0 || capacity < READING_WIRE_SIZE) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    Telemetry reading_in;
    if (!Telemetry_deserialize(telemetry, (size_t)size, &reading_in)) {
        return ROBOHUB_ERR_PROTOCOL;
    }

    /* The same reading, in the uplink's shape: the device, time, temperature and
     * status, with the status narrowed to the uplink's u16. */
    Reading reading;
    memset(&reading, 0, sizeof(reading));
    reading.device = reading_in.device;
    reading.timestamp_ms = reading_in.timestamp_ms;
    reading.temperature_c = reading_in.temperature_c;
    reading.status = (rh_u16)reading_in.status;

    if (Reading_serialize(&reading, out, (size_t)capacity) == 0u) {
        return ROBOHUB_ERR_PROTOCOL;
    }
    return ROBOHUB_OK;
}

RoboHubError robohub_uplink_send_telemetry(RoboHubRelayUplink* uplink, const rh_u8* payload, rh_size size)
{
    if (uplink == 0 || uplink->link == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    rh_u8 bytes[READING_WIRE_SIZE];
    RoboHubError err = robohub_uplink_reframe(payload, size, bytes, (rh_size)sizeof(bytes));
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    return robohub_transport_send(uplink->link, MESSAGE_ID_READING, 0u, bytes, (rh_u16)READING_WIRE_SIZE);
}
