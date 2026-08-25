#define _POSIX_C_SOURCE 200809L

#include "commands.h"
#include "crc.h"
#include "transport.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>


static TRANSPORT g_transport;
static uint8_t g_next_sequence = 0;
static volatile sig_atomic_t g_watch_stop = 0;

static void handle_sigint(int signum) {
    (void)signum;
    g_watch_stop = 1;
}

#define WAIT_MAX_ATTEMPTS 5
#define BOOTLOADER_CHUNK_MAX_RETRIES 3
#define WIRE_BUF_MAX (5 + PAYLOAD_MAX_SIZE + 2)

static CMD_RESULT send_frame(
    uint8_t message_id,
    const void *payload,
    uint8_t payload_len,
    uint8_t sequence,
    int *err_details
) {
    if (payload_len > PAYLOAD_MAX_SIZE) {
        if (err_details) *err_details = TRANSPORT_ERR_INVALID_ARGS;
        return CMD_ERR_TRANSPORT;
    }

    uint8_t buf[WIRE_BUF_MAX];
    size_t offset = 0;

    buf[offset++] = PROTOCOL_START_BYTE;
    buf[offset++] = PROTOCOL_VERSION;
    buf[offset++] = message_id;
    buf[offset++] = sequence;
    buf[offset++] = payload_len;

    if (payload_len > 0 && payload != NULL) {
        memcpy(buf + offset, payload, payload_len);
    }
    offset += payload_len;

    uint16_t crc = compute_crc16(buf, offset);
    buf[offset++] = (uint8_t)(crc >> 8);
    buf[offset++] = (uint8_t)(crc & 0xFF);

    int rc = transport_send(&g_transport, buf, offset);
    if (rc != TRANSPORT_OK) {
        if (err_details) *err_details = rc;
        return CMD_ERR_TRANSPORT;
    }

    return CMD_OK;
}

static CMD_RESULT recv_frame(
    uint8_t *out_message_id,
    uint8_t *out_sequence,
    uint8_t *out_payload,
    uint8_t *out_payload_len,
    int *err_details
) {
    uint8_t header[5];
    int rc = transport_recv_exact(&g_transport, header, sizeof(header));
    if (rc != TRANSPORT_OK) {
        if (err_details) *err_details = rc;
        return (rc == TRANSPORT_ERR_TIMEOUT) ? CMD_ERR_NO_RESPONSE : CMD_ERR_TRANSPORT;
    }

    uint8_t start_byte  = header[0];
    uint8_t version     = header[1];
    uint8_t message_id  = header[2];
    uint8_t sequence    = header[3];
    uint8_t payload_len = header[4];

    if (start_byte != PROTOCOL_START_BYTE || version != PROTOCOL_VERSION) {
        if (err_details) *err_details = TRANSPORT_ERR_SOCKET;
        return CMD_ERR_TRANSPORT;
    }

    if (payload_len > PAYLOAD_MAX_SIZE) {
        if (err_details) *err_details = TRANSPORT_ERR_SOCKET;
        return CMD_ERR_TRANSPORT;
    }

    uint8_t payload[PAYLOAD_MAX_SIZE];
    if (payload_len > 0) {
        rc = transport_recv_exact(&g_transport, payload, payload_len);
        if (rc != TRANSPORT_OK) {
            if (err_details) *err_details = rc;
            return (rc == TRANSPORT_ERR_TIMEOUT) ? CMD_ERR_NO_RESPONSE : CMD_ERR_TRANSPORT;
        }
    }

    uint8_t crc_bytes[2];
    rc = transport_recv_exact(&g_transport, crc_bytes, sizeof(crc_bytes));
    if (rc != TRANSPORT_OK) {
        if (err_details) *err_details = rc;
        return (rc == TRANSPORT_ERR_TIMEOUT) ? CMD_ERR_NO_RESPONSE : CMD_ERR_TRANSPORT;
    }
    uint16_t received_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];

    uint8_t crc_buf[WIRE_BUF_MAX];
    size_t crc_len = 0;
    crc_buf[crc_len++] = start_byte;
    crc_buf[crc_len++] = version;
    crc_buf[crc_len++] = message_id;
    crc_buf[crc_len++] = sequence;
    crc_buf[crc_len++] = payload_len;
    if (payload_len > 0) {
        memcpy(crc_buf + crc_len, payload, payload_len);
        crc_len += payload_len;
    }

    if (compute_crc16(crc_buf, crc_len) != received_crc) {
        if (err_details) *err_details = PROTO_ERR_CRC_FAIL;
        return CMD_ERR_TRANSPORT;
    }

    *out_message_id = message_id;
    *out_sequence = sequence;
    *out_payload_len = payload_len;
    if (payload_len > 0) {
        memcpy(out_payload, payload, payload_len);
    }

    return CMD_OK;
}

static CMD_RESULT wait_for_ack(uint8_t sequence, int *err_details) {
    for (int attempt = 0; attempt < WAIT_MAX_ATTEMPTS; attempt++) {
        uint8_t msg_id, seq, payload_len;
        uint8_t payload[PAYLOAD_MAX_SIZE];

        CMD_RESULT rc = recv_frame(&msg_id, &seq, payload, &payload_len, err_details);
        if (rc == CMD_ERR_TRANSPORT) return rc;
        if (rc == CMD_ERR_NO_RESPONSE) continue; /* one recv timed out, try again */

        if (msg_id == MSG_ACK) {
            ACK_PAYLOAD ack;
            memcpy(&ack, payload, sizeof(ack));
            if (ack.ack_seq == sequence) {
                return CMD_OK;
            }
            continue; /* stale ACK for a different sequence */
        }

        if (msg_id == MSG_NACK) {
            NACK_PAYLOAD nack;
            memcpy(&nack, payload, sizeof(nack));
            if (nack.nacked_seq == sequence) {
                if (err_details) *err_details = nack.error;
                return CMD_ERR_NACKED;
            }
            continue;
        }
    }

    if (err_details) *err_details = TRANSPORT_ERR_TIMEOUT;
    return CMD_ERR_NO_RESPONSE;
}

static CMD_RESULT wait_for_message(
    uint8_t expected_message_id,
    void *out_payload,
    size_t out_payload_size,
    int *err_details
) {
    for (int attempt = 0; attempt < WAIT_MAX_ATTEMPTS; attempt++) {
        uint8_t msg_id, seq, payload_len;
        uint8_t payload[PAYLOAD_MAX_SIZE];

        CMD_RESULT rc = recv_frame(&msg_id, &seq, payload, &payload_len, err_details);
        if (rc == CMD_ERR_TRANSPORT) return rc;
        if (rc == CMD_ERR_NO_RESPONSE) continue;

        if (msg_id == expected_message_id) {
            if (payload_len != out_payload_size) {
                if (err_details) *err_details = PROTO_ERR_PAYLOAD_OVERSIZE;
                return CMD_ERR_TRANSPORT;
            }
            memcpy(out_payload, payload, out_payload_size);
            return CMD_OK;
        }

        if (msg_id == MSG_NACK) {
            NACK_PAYLOAD nack;
            memcpy(&nack, payload, sizeof(nack));
            if (err_details) *err_details = nack.error;
            return CMD_ERR_NACKED;
        }

        /* something else unrelated - ignore, keep waiting */
    }

    if (err_details) *err_details = TRANSPORT_ERR_TIMEOUT;
    return CMD_ERR_NO_RESPONSE;
}

static CMD_RESULT send_and_wait_ack(
    uint8_t message_id,
    const void *payload,
    uint8_t payload_len,
    int *err_details
) {
    uint8_t sequence = g_next_sequence++;

    CMD_RESULT rc = send_frame(message_id, payload, payload_len, sequence, err_details);
    if (rc != CMD_OK) {
        return rc;
    }

    return wait_for_ack(sequence, err_details);
}

CMD_RESULT cmd_init(const char *host, uint16_t port, int *err_details) {
    compute_crc16_table();

    int rc = transport_connect(&g_transport, host, port);
    if (rc != TRANSPORT_OK) {
        if (err_details) *err_details = rc;
        return CMD_ERR_TRANSPORT;
    }

    g_next_sequence = 0;
    g_watch_stop = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    return CMD_OK;
}

CMD_RESULT cmd_wait_heartbeat(int *err_details) {
    HEARTBEAT_PAYLOAD hb;
    return wait_for_message(MSG_HEARTBEAT, &hb, sizeof(hb), err_details);
}

CMD_RESULT cmd_set_flight_state(FLIGHT_STATE state, int *err_details) {
    FLIGHT_STATE_PAYLOAD payload;
    payload.requested_state = (uint8_t)state;
    return send_and_wait_ack(MSG_FLIGHT_STATE, &payload, sizeof(payload), err_details);
}

CMD_RESULT cmd_arm(int *err_details) {
    return cmd_set_flight_state(FLIGHT_ARMED, err_details);
}

CMD_RESULT cmd_disarm(int *err_details) {
    return cmd_set_flight_state(FLIGHT_DISARMED, err_details);
}

CMD_RESULT cmd_set_flight_mode(FLIGHT_MODE mode, int *err_details) {
    FLIGHT_MODE_PAYLOAD payload;
    payload.requested_mode = (uint8_t)mode;
    return send_and_wait_ack(MSG_FLIGHT_MODE, &payload, sizeof(payload), err_details);
}

static CMD_RESULT bootloader_cmd(BOOTLOADER_CMD cmd, uint32_t addr, uint16_t len, int *err_details) {
    BOOTLOADER_CMD_PAYLOAD payload;
    memset(&payload, 0, sizeof(payload));
    payload.addr = addr;
    payload.len = len;
    payload.cmd = (uint8_t)cmd;
    
    return send_and_wait_ack(MSG_BOOTLOADER_CMD, &payload, sizeof(payload), err_details);
}

CMD_RESULT cmd_bootloader_stats(int *err_details) {
    return bootloader_cmd(BOOTLOADER_STATS, 0, 0, err_details);
}

CMD_RESULT cmd_bootloader_erase_app(int *err_details) {
    return bootloader_cmd(BOOTLOADER_ERASE_APP, 0, 0, err_details);
}

CMD_RESULT cmd_bootloader_verify(int *err_details) {
    return bootloader_cmd(BOOTLOADER_VERIFY, 0, 0, err_details);
}

CMD_RESULT cmd_bootloader_update(uint8_t *data, size_t data_size, int *err_details) {
    if (data == NULL || data_size == 0) {
        if (err_details) *err_details = TRANSPORT_ERR_INVALID_ARGS;
        return CMD_ERR_TRANSPORT;
    }
    if (data_size > 0xFFFF) {
        if (err_details) *err_details = TRANSPORT_ERR_INVALID_ARGS;
        return CMD_ERR_TRANSPORT;
    }

    /* Tell the FC an update is starting and how big it'll be. */
    CMD_RESULT rc = bootloader_cmd(BOOTLOADER_UPDATE, 0, (uint16_t)data_size, err_details);
    if (rc != CMD_OK) {
        return rc;
    }

    size_t offset = 0;
    while (offset < data_size) {
        size_t chunk_len = data_size - offset;
        if (chunk_len > sizeof(((BOOTLOADER_DATA_PAYLOAD *)0)->data)) {
            chunk_len = sizeof(((BOOTLOADER_DATA_PAYLOAD *)0)->data);
        }

        BOOTLOADER_DATA_PAYLOAD payload;
        memset(&payload, 0, sizeof(payload));
        payload.addr = (uint32_t)offset;
        memcpy(payload.data, data + offset, chunk_len);

        CMD_RESULT chunk_rc = CMD_ERR_NO_RESPONSE;
        int retry;
        for (retry = 0; retry < BOOTLOADER_CHUNK_MAX_RETRIES; retry++) {
            chunk_rc = send_and_wait_ack(MSG_BOOTLOADER_DATA, &payload, sizeof(payload), err_details);
            if (chunk_rc == CMD_OK) {
                break;
            }
            if (chunk_rc == CMD_ERR_TRANSPORT) {
                /* link is down - retrying won't help, bail out now */
                return chunk_rc;
            }
            /* CMD_ERR_NO_RESPONSE or CMD_ERR_NACKED - worth one more try */
        }

        if (chunk_rc != CMD_OK) {
            return chunk_rc; /* gave up on this chunk after retries */
        }

        offset += chunk_len;
    }

    return CMD_OK;
}

CMD_RESULT cmd_wait_imu_telem(IMU *imu, int *err_details) {
    TELEM_IMU_PAYLOAD payload;
    CMD_RESULT rc = wait_for_message(MSG_TELEM_IMU, &payload, sizeof(payload), err_details);
    if (rc == CMD_OK) *imu = payload.imu;
    return rc;
}

CMD_RESULT cmd_wait_gps_telem(GPS *gps, int *err_details) {
    TELEM_GPS_PAYLOAD payload;
    CMD_RESULT rc = wait_for_message(MSG_TELEM_GPS, &payload, sizeof(payload), err_details);
    if (rc == CMD_OK) *gps = payload.gps;
    return rc;
}

CMD_RESULT cmd_wait_barometer_telem(BAROMETER *barometer, int *err_details) {
    TELEM_BAROMETER_PAYLOAD payload;
    CMD_RESULT rc = wait_for_message(MSG_TELEM_BAROMETER, &payload, sizeof(payload), err_details);
    if (rc == CMD_OK) *barometer = payload.barometer;
    return rc;
}

CMD_RESULT cmd_wait_power_telem(POWER *power, int *err_details) {
    TELEM_POWER_PAYLOAD payload;
    CMD_RESULT rc = wait_for_message(MSG_TELEM_POWER, &payload, sizeof(payload), err_details);
    if (rc == CMD_OK) *power = payload.power;
    return rc;
}

CMD_RESULT cmd_watch_imu(IMU *imu, int *err_details) {
    g_watch_stop = 0;
    while (!g_watch_stop) {
        CMD_RESULT rc = cmd_wait_imu_telem(imu, err_details);
        if (rc == CMD_ERR_TRANSPORT) return rc;
        if (rc == CMD_OK) {
            printf("[IMU] t=%u accel=(%.3f, %.3f, %.3f) gyro=(%.3f, %.3f, %.3f)\n",
                   imu->timestamp,
                   imu->acceleration.x, imu->acceleration.y, imu->acceleration.z,
                   imu->gyro.x, imu->gyro.y, imu->gyro.z);
        }
        /* CMD_ERR_NO_RESPONSE / CMD_ERR_NACKED: just loop again */
    }
    return CMD_OK;
}

CMD_RESULT cmd_watch_gps(GPS *gps, int *err_details) {
    g_watch_stop = 0;
    while (!g_watch_stop) {
        CMD_RESULT rc = cmd_wait_gps_telem(gps, err_details);
        if (rc == CMD_ERR_TRANSPORT) return rc;
        if (rc == CMD_OK) {
            printf("[GPS] lat=%d lon=%d alt_sea=%.2f alt_gnd=%.2f fix=%d\n",
                   gps->latitude, gps->longitude,
                   gps->altitude_meters_abv_sealvl, gps->altitude_meters_abv_ground,
                   gps->fix_type);
        }
    }
    return CMD_OK;
}

CMD_RESULT cmd_watch_barometer(BAROMETER *barometer, int *err_details) {
    g_watch_stop = 0;
    while (!g_watch_stop) {
        CMD_RESULT rc = cmd_wait_barometer_telem(barometer, err_details);
        if (rc == CMD_ERR_TRANSPORT) return rc;
        if (rc == CMD_OK) {
            printf("[BARO] pressure=%.2f Pa temp=%.2f C alt=%.2f m\n",
                   barometer->pressure_pa, barometer->temp_c, barometer->alt_m);
        }
    }
    return CMD_OK;
}

CMD_RESULT cmd_watch_power(POWER *power, int *err_details) {
    g_watch_stop = 0;
    while (!g_watch_stop) {
        CMD_RESULT rc = cmd_wait_power_telem(power, err_details);
        if (rc == CMD_ERR_TRANSPORT) return rc;
        if (rc == CMD_OK) {
            printf("[POWER] %.2fV %.2fA %.1fmAh %u%%\n",
                   power->voltage, power->current, power->consumed_mah, power->percent);
        }
    }
    return CMD_OK;
}

CMD_RESULT cmd_watch_overall(int *err_details) {
    g_watch_stop = 0;

    while (!g_watch_stop) {
        uint8_t msg_id, seq, payload_len;
        uint8_t payload[PAYLOAD_MAX_SIZE];

        CMD_RESULT rc = recv_frame(&msg_id, &seq, payload, &payload_len, err_details);
        if (rc == CMD_ERR_TRANSPORT) return rc;
        if (rc != CMD_OK) continue; /* no data this attempt, keep watching */

        switch (msg_id) {
            case MSG_HEARTBEAT: {
                HEARTBEAT_PAYLOAD hb;
                memcpy(&hb, payload, sizeof(hb));
                printf("[HEARTBEAT] state=%u mode=%u flags=0x%04x\n",
                       hb.state, hb.mode, hb.error_flags);
                break;
            }
            case MSG_TELEM_IMU: {
                TELEM_IMU_PAYLOAD t;
                memcpy(&t, payload, sizeof(t));
                printf("[IMU] accel=(%.2f, %.2f, %.2f)\n",
                       t.imu.acceleration.x, t.imu.acceleration.y, t.imu.acceleration.z);
                break;
            }
            case MSG_TELEM_GPS: {
                TELEM_GPS_PAYLOAD t;
                memcpy(&t, payload, sizeof(t));
                printf("[GPS] lat=%d lon=%d\n", t.gps.latitude, t.gps.longitude);
                break;
            }
            case MSG_TELEM_BAROMETER: {
                TELEM_BAROMETER_PAYLOAD t;
                memcpy(&t, payload, sizeof(t));
                printf("[BARO] alt=%.2f m\n", t.barometer.alt_m);
                break;
            }
            case MSG_TELEM_POWER: {
                TELEM_POWER_PAYLOAD t;
                memcpy(&t, payload, sizeof(t));
                printf("[POWER] %u%%\n", t.power.percent);
                break;
            }
            default:
                break;
        }
    }

    return CMD_OK;
}