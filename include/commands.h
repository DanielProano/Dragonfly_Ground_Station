#ifndef _COMMANDS_
#define _COMMANDS_

#include "protocol.h"
#include <stddef.h>

typedef enum {
    CMD_OK,
    CMD_ERR_TRANSPORT,
    CMD_ERR_NO_RESPONSE,
    CMD_ERR_NACKED,
} CMD_RESULT;

CMD_RESULT cmd_init(const char *host, uint16_t port, int *err_details);
CMD_RESULT cmd_wait_heartbeat(int *err_details);
CMD_RESULT cmd_arm(int *err_details);
CMD_RESULT cmd_disarm(int *err_details);
CMD_RESULT cmd_set_flight_state(FLIGHT_STATE state, int *err_details);
CMD_RESULT cmd_set_flight_mode(FLIGHT_MODE mode, int *err_details);
CMD_RESULT cmd_bootloader_stats(int *err_details);
CMD_RESULT cmd_bootloader_erase_app(int *err_details);
CMD_RESULT cmd_bootloader_update(uint8_t *data, size_t data_size, int *err_details);
CMD_RESULT cmd_bootloader_verify(int *err_details);
CMD_RESULT cmd_wait_imu_telem(IMU *imu, int *err_details);
CMD_RESULT cmd_wait_gps_telem(GPS *gps, int *err_details);
CMD_RESULT cmd_wait_barometer_telem(BAROMETER *barometer, int *err_details);
CMD_RESULT cmd_wait_power_telem(POWER *power, int *err_details);
CMD_RESULT cmd_esp32_status(ESP32_STATUS_PAYLOAD *status, int *err_details);

CMD_RESULT cmd_watch_imu(IMU *imu, int *err_details);
CMD_RESULT cmd_watch_gps(GPS *gps, int *err_details);
CMD_RESULT cmd_watch_barometer(BAROMETER *barometer, int *err_details);
CMD_RESULT cmd_watch_power(POWER *power, int *err_details);
CMD_RESULT cmd_watch_overall(int *err_details);

#endif