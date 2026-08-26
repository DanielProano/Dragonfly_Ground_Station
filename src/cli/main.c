#define _POSIX_C_SOURCE 200809L

#include "commands.h"
#include "protocol.h"
#include "transport.h"

#include <errno.h>
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
        "  bl-update <file>     Upload firmware image\n"
        "  bl-verify            Verify application\n",
        prog);
}

/*
 * Every command handler gets the args after the command name (argv[0] here
 * is the first extra arg, not the program name) plus a slot for protocol/
 * transport error details. Handlers do their own printing on success.
 */
typedef CMD_RESULT (*CmdHandler)(int argc, char **argv, int *err_details);

static CMD_RESULT h_arm(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    return cmd_arm(err);
}

static CMD_RESULT h_disarm(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    return cmd_disarm(err);
}

static CMD_RESULT h_mode(int argc, char **argv, int *err) {
    if (argc < 1) {
        fprintf(stderr, "Error: mode requires an argument\n");
        *err = TRANSPORT_ERR_INVALID_ARGS;
        return CMD_ERR_TRANSPORT;
    }
    return cmd_set_flight_mode((FLIGHT_MODE)atoi(argv[0]), err);
}

static CMD_RESULT h_status(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    CMD_RESULT rc = cmd_wait_heartbeat(err);
    if (rc == CMD_OK) printf("Heartbeat OK\n");
    return rc;
}

static CMD_RESULT h_watch_imu(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    IMU imu;
    return cmd_watch_imu(&imu, err);
}

static CMD_RESULT h_watch_gps(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    GPS gps;
    return cmd_watch_gps(&gps, err);
}

static CMD_RESULT h_watch_baro(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    BAROMETER baro;
    return cmd_watch_barometer(&baro, err);
}

static CMD_RESULT h_watch_power(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    POWER pwr;
    return cmd_watch_power(&pwr, err);
}

static CMD_RESULT h_watch(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    return cmd_watch_overall(err);
}

static CMD_RESULT h_bl_stats(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    return cmd_bootloader_stats(err);
}

static CMD_RESULT h_bl_erase(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    return cmd_bootloader_erase_app(err);
}

static CMD_RESULT h_bl_verify(int argc, char **argv, int *err) {
    (void)argc; (void)argv;
    return cmd_bootloader_verify(err);
}

/* Reads a whole file into a malloc'd buffer. Prints its own error and
 * returns NULL on failure. Caller owns the returned buffer. */
static uint8_t *read_file(const char *path, long *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: could not open '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size <= 0) {
        fprintf(stderr, "Error: could not determine size of '%s'\n", path);
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    uint8_t *data = malloc((size_t)size);
    if (!data) {
        fprintf(stderr, "Error: out of memory reading '%s'\n", path);
        fclose(fp);
        return NULL;
    }

    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        fprintf(stderr, "Error: failed to read '%s'\n", path);
        fclose(fp);
        free(data);
        return NULL;
    }
    fclose(fp);

    *out_size = size;
    return data;
}

static CMD_RESULT h_bl_update(int argc, char **argv, int *err) {
    if (argc < 1) {
        fprintf(stderr, "Error: bl-update requires a firmware file path\n");
        *err = TRANSPORT_ERR_INVALID_ARGS;
        return CMD_ERR_TRANSPORT;
    }

    long size = 0;
    uint8_t *data = read_file(argv[0], &size);
    if (!data) {
        *err = TRANSPORT_ERR_INVALID_ARGS;
        return CMD_ERR_TRANSPORT;
    }

    printf("Uploading %ld bytes from %s...\n", size, argv[0]);
    CMD_RESULT rc = cmd_bootloader_update(data, (size_t)size, err);
    free(data);
    return rc;
}

static const struct {
    const char *name;
    CmdHandler handler;
} COMMANDS[] = {
    {"arm",         h_arm},
    {"disarm",      h_disarm},
    {"mode",        h_mode},
    {"status",      h_status},
    {"watch-imu",   h_watch_imu},
    {"watch-gps",   h_watch_gps},
    {"watch-baro",  h_watch_baro},
    {"watch-power", h_watch_power},
    {"watch",       h_watch},
    {"bl-stats",    h_bl_stats},
    {"bl-erase",    h_bl_erase},
    {"bl-update",   h_bl_update},
    {"bl-verify",   h_bl_verify},
};

static CmdHandler find_handler(const char *name) {
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
        if (strcmp(name, COMMANDS[i].name) == 0) {
            return COMMANDS[i].handler;
        }
    }
    return NULL;
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

    const char *cmd = argv[optind];
    CmdHandler handler = find_handler(cmd);
    if (!handler) {
        fprintf(stderr, "Unknown command: %s\n\n", cmd);
        usage(argv[0]);
        return 1;
    }

    printf("Connecting to %s:%u...\n", host, port);

    int err = 0;
    CMD_RESULT rc = cmd_init(host, port, &err);
    if (rc != CMD_OK) {
        print_error("Error while connecting", rc, err);
        return 1;
    }

    rc = handler(argc - optind - 1, argv + optind + 1, &err);
    if (rc != CMD_OK) {
        print_error(cmd, rc, err);
        return 1;
    }

    return 0;
}