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
CMD_RESULT cmd_wait_heartbeat(heartbeat_payload *hb, int *err_details);
CMD_RESULT cmd_set_flight_state(flight_state state, int *err_details);
CMD_RESULT cmd_set_flight_mode(flight_mode mode, int *err_details);
CMD_RESULT cmd_arm(int *err_details);
CMD_RESULT cmd_disarm(int *err_details);
CMD_RESULT cmd_bootloader_stats(bootloader_stats_payload *paylaod, int *err_details);
CMD_RESULT cmd_bootloader_erase_app(int *err_details);
CMD_RESULT cmd_bootloader_update(uint8_t *data, uint16_t data_size, int *err_details);
CMD_RESULT cmd_bootloader_verify(int *err_details);
CMD_RESULT cmd_wait_imu_telem(imu *imu, int *err_details);
CMD_RESULT cmd_wait_barometer_telem(barometer *barometer, int *err_details);
CMD_RESULT cmd_wait_power_telem(power *power, int *err_details);
CMD_RESULT cmd_esp32_status(esp32_status_payload *status, int *err_details);
CMD_RESULT cmd_esp32_oled_print(char *msg, size_t msg_len, int *err_details);
CMD_RESULT cmd_esp32_oled_clear(int *err_details);
CMD_RESULT cmd_motor_throttle(const uint8_t percent[4], int *err_details);
CMD_RESULT cmd_run_motors(const uint8_t percent[4], int *err_details);

CMD_RESULT cmd_watch_imu(imu *imu, int *err_details);
CMD_RESULT cmd_watch_barometer(barometer *barometer, int *err_details);
CMD_RESULT cmd_watch_power(power *power, int *err_details);
CMD_RESULT cmd_watch_overall(int *err_details);

#endif