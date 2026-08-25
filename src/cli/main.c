#define _POSIX_C_SOURCE 200809L

#include "commands.h"
#include "protocol.h"
#include "transport.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_error(const char *ctx, CMD_RESULT rc, int err)
{
    fprintf(stderr, "%s failed: ", ctx);

    if (rc == CMD_ERR_TRANSPORT) {
        switch (err) {
            case TRANSPORT_ERR_INVALID_ARGS:  fprintf(stderr, "invalid args\n"); break;
            case TRANSPORT_ERR_CONNECT:       fprintf(stderr, "connection failed\n"); break;
            case TRANSPORT_ERR_SOCKET:        fprintf(stderr, "socket error\n"); break;
            case TRANSPORT_ERR_TIMEOUT:       fprintf(stderr, "timeout\n"); break;
            case TRANSPORT_ERR_CLOSED:        fprintf(stderr, "connection closed\n"); break;
            default:                          fprintf(stderr, "transport code %d\n", err); break;
        }
        return;
    }

    if (rc == CMD_ERR_NACKED) {
        switch (err) {
            case PROTO_ERR_CRC_FAIL:          fprintf(stderr, "CRC mismatch\n"); break;
            case PROTO_ERR_UNKNOWN_MSG:       fprintf(stderr, "unknown message\n"); break;
            case PROTO_ERR_WRONG_VERSION:     fprintf(stderr, "protocol version mismatch\n"); break;
            case PROTO_ERR_PAYLOAD_OVERSIZE:  fprintf(stderr, "payload oversize\n"); break;
            case PROTO_ERR_INVALID_STATE:     fprintf(stderr, "invalid state\n"); break;
            case PROTO_ERR_BUFFER_FULL:       fprintf(stderr, "buffer full\n"); break;
            case PROTO_ERR_AUTH_FAIL:         fprintf(stderr, "auth failed\n"); break;
            case PROTO_ERR_FLASH_FAIL:        fprintf(stderr, "flash failed\n"); break;
            case PROTO_ERR_SENSOR_FAIL:       fprintf(stderr, "sensor failed\n"); break;
            case PROTO_ERR_TIMEOUT:           fprintf(stderr, "protocol timeout\n"); break;
            default:                    fprintf(stderr, "protocol code %d\n", err); break;
        }
        return;
    }

    /* CMD_ERR_NO_RESPONSE: err just echoes TRANSPORT_ERR_TIMEOUT, but
     * "no response" is a more accurate message than "timeout" alone. */
    fprintf(stderr, "no response from flight controller\n");
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] <command> [args]\n\n"
        "Options:\n"
        "  -h, --host <addr>   ESP32 IP (default: 192.168.4.1)\n"
        "  -p, --port <num>    ESP32 port (default: 8080)\n\n"
        "Commands:\n"
        "  arm                  Arm the drone\n"
        "  disarm               Disarm the drone\n"
        "  mode <n>             Set flight mode:\n"
        "                         0=manual 1=acro 2=auto 3=wp 4=mission 5=rtl 6=land\n"
        "  status               Request heartbeat\n"
        "  watch-imu            Stream IMU telemetry (Ctrl+C to stop)\n"
        "  watch-gps            Stream GPS telemetry (Ctrl+C to stop)\n"
        "  watch-baro           Stream barometer telemetry (Ctrl+C to stop)\n"
        "  watch-power          Stream power telemetry (Ctrl+C to stop)\n"
        "  watch                Stream all telemetry (Ctrl+C to stop)\n"
        "  bl-stats             Bootloader info\n"
        "  bl-erase             Erase application flash\n"
        "  bl-verify            Verify application\n",
        prog);
}

int main(int argc, char **argv)
{
    const char *host = "192.168.4.1";
    uint16_t port = 8080;

    static struct option long_opts[] = {
        {"host", required_argument, NULL, 'h'},
        {"port", required_argument, NULL, 'p'},
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "h:p:", long_opts, NULL)) != -1) {
        switch (c) {
            case 'h': host = optarg; break;
            case 'p': port = (uint16_t)atoi(optarg); break;
            default:  {
                usage(argv[0]); 
                fprintf(stderr, "Could not connect");
                return 1;
            }
        }
    }

    if (optind >= argc) {
        usage(argv[0]);
        fprintf(stderr, "Could not connect");
        return 1;
    }

    printf("Connecting to %s:%u...\n", host, port);

    const char *cmd = argv[optind];
    int err = 0;
    CMD_RESULT rc = cmd_init(host, port, &err);

    if (rc != CMD_OK) {
        print_error("Error while connecting", rc, err);
        return 1;
    }

    if (strcmp(cmd, "arm") == 0) {
        rc = cmd_arm(&err);
    }
    else if (strcmp(cmd, "disarm") == 0) {
        rc = cmd_disarm(&err);
    }
    else if (strcmp(cmd, "mode") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Error: mode requires an argument\n");
            return 1;
        }
        rc = cmd_set_flight_mode((FLIGHT_MODE)atoi(argv[optind + 1]), &err);
    }
    else if (strcmp(cmd, "status") == 0) {
        rc = cmd_wait_heartbeat(&err);
        if (rc == CMD_OK) printf("Heartbeat OK\n");
    }
    else if (strcmp(cmd, "watch-imu") == 0) {
        IMU imu;
        rc = cmd_watch_imu(&imu, &err);
    }
    else if (strcmp(cmd, "watch-gps") == 0) {
        GPS gps;
        rc = cmd_watch_gps(&gps, &err);
    }
    else if (strcmp(cmd, "watch-baro") == 0) {
        BAROMETER baro;
        rc = cmd_watch_barometer(&baro, &err);
    }
    else if (strcmp(cmd, "watch-power") == 0) {
        POWER pwr;
        rc = cmd_watch_power(&pwr, &err);
    }
    else if (strcmp(cmd, "watch") == 0) {
        rc = cmd_watch_overall(&err);
    }
    else if (strcmp(cmd, "bl-stats") == 0) {
        rc = cmd_bootloader_stats(&err);
    }
    else if (strcmp(cmd, "bl-erase") == 0) {
        rc = cmd_bootloader_erase_app(&err);
    }
    else if (strcmp(cmd, "bl-verify") == 0) {
        rc = cmd_bootloader_verify(&err);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n\n", cmd);
        usage(argv[0]);
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* Result                                                             */
    /* ------------------------------------------------------------------ */

    if (rc != CMD_OK) {
        print_error(cmd, rc, err);
        return 1;
    }

    return 0;
}