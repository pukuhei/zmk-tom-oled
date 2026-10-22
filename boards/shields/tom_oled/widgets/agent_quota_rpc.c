/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * tom_oled__agent_quota request v1, 24 bytes, little endian:
 *   0  version (1)
 *   1  source (0 = Codex, 1 = Claude)
 *   2  primary remaining percent (0..100, 0xFF = unknown)
 *   3  secondary remaining percent (0..100, 0xFF = unknown)
 *   4  u16 primary window minutes (0 = unknown)
 *   6  u16 secondary window minutes (0 = unknown)
 *   8  u32 seconds until the primary window resets (0xFFFFFFFF = unknown)
 *  12  u16 TTL seconds (0 = default)
 *  14  flags (bit 0 = clear to NO DATA)
 *  15  reserved (0)
 *  16  u32 session
 *  20  u32 sequence
 * Response, 4 bytes: version, code, fresh (0/1), reserved.
 */

#include "agent_quota.h"

#include <pb_encode.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/byteorder.h>
#include <zmk/studio/custom.h>

#define AGENT_QUOTA_PROTOCOL_VERSION 1
#define AGENT_QUOTA_REQUEST_SIZE 24
#define AGENT_QUOTA_RESPONSE_SIZE 4
#define AGENT_QUOTA_FLAG_CLEAR BIT(0)

enum response_code {
    RESPONSE_OK = 0,
    RESPONSE_INVALID_LENGTH = 1,
    RESPONSE_INVALID_VERSION = 2,
    RESPONSE_INVALID_VALUE = 3,
    RESPONSE_STALE_SEQUENCE = 4,
};

static struct zmk_rpc_custom_subsystem_meta agent_quota_meta = {
    .ui_urls = NULL,
    .ui_urls_count = 0,
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

static uint8_t response_payload[AGENT_QUOTA_RESPONSE_SIZE];
static bool have_sequence;
static uint32_t current_session;
static uint32_t current_sequence;

static bool encode_response_payload(pb_ostream_t *stream, const pb_field_t *field,
                                    void *const *arg) {
    const uint8_t *payload = *arg;
    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_string(stream, payload, AGENT_QUOTA_RESPONSE_SIZE);
}

static void prepare_response(pb_callback_t *encode_response, enum response_code code) {
    struct zmk_tom_oled_quota quota;
    response_payload[0] = AGENT_QUOTA_PROTOCOL_VERSION;
    response_payload[1] = code;
    response_payload[2] = zmk_tom_oled_agent_quota_get(&quota) ? 1 : 0;
    response_payload[3] = 0;
    encode_response->funcs.encode = encode_response_payload;
    encode_response->arg = response_payload;
}

static bool sequence_is_newer(uint32_t session, uint32_t sequence) {
    if (!have_sequence || session != current_session) {
        return true;
    }
    return (int32_t)(sequence - current_sequence) > 0;
}

ZMK_RPC_CUSTOM_SUBSYSTEM(tom_oled__agent_quota, &agent_quota_meta,
                         agent_quota_rpc_handle_request);

static bool agent_quota_rpc_handle_request(const zmk_custom_CallRequest *request,
                                           pb_callback_t *encode_response) {
    if (request->payload.size != AGENT_QUOTA_REQUEST_SIZE) {
        prepare_response(encode_response, RESPONSE_INVALID_LENGTH);
        return true;
    }

    const uint8_t *payload = request->payload.bytes;
    if (payload[0] != AGENT_QUOTA_PROTOCOL_VERSION) {
        prepare_response(encode_response, RESPONSE_INVALID_VERSION);
        return true;
    }

    struct zmk_tom_oled_quota quota = {
        .source = payload[1],
        .primary_remaining = payload[2],
        .secondary_remaining = payload[3],
        .primary_window_minutes = sys_get_le16(&payload[4]),
        .secondary_window_minutes = sys_get_le16(&payload[6]),
        .primary_reset_seconds = sys_get_le32(&payload[8]),
    };
    uint16_t ttl_seconds = sys_get_le16(&payload[12]);
    uint8_t flags = payload[14];
    uint32_t session = sys_get_le32(&payload[16]);
    uint32_t sequence = sys_get_le32(&payload[20]);

    if (!sequence_is_newer(session, sequence)) {
        prepare_response(encode_response, RESPONSE_STALE_SEQUENCE);
        return true;
    }

    if (flags & AGENT_QUOTA_FLAG_CLEAR) {
        zmk_tom_oled_agent_quota_clear();
    } else {
        if (ttl_seconds == 0) {
            ttl_seconds = CONFIG_ZMK_TOM_OLED_AGENT_QUOTA_DEFAULT_TTL_SEC;
        } else if (ttl_seconds > CONFIG_ZMK_TOM_OLED_AGENT_QUOTA_MAX_TTL_SEC) {
            ttl_seconds = CONFIG_ZMK_TOM_OLED_AGENT_QUOTA_MAX_TTL_SEC;
        }
        if (zmk_tom_oled_agent_quota_set(&quota, ttl_seconds) != 0) {
            prepare_response(encode_response, RESPONSE_INVALID_VALUE);
            return true;
        }
    }

    current_session = session;
    current_sequence = sequence;
    have_sequence = true;
    prepare_response(encode_response, RESPONSE_OK);
    return true;
}
