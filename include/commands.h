#ifndef _COMMANDS_
#define _COMMANDS_

#include "protocol.h"
#include <stddef.h>

static int socket_fd;

int cmd_init(int socket_fd);
int cmd_request_heartbeat(void);
int cmd_arm(void);
int cmd_disarm(void);
int cmd_set_flight_state(FLIGHT_STATE state);
int cmd_set_flight_mode(FLIGHT_MODE mode);
int cmd_bootloader_stats(void);
int cmd_bootloader_erase_app(void);
int cmd_bootloader_update(uint8_t *data, size_t data_size);
int cmd_bootloader_verify(void);
int cmd_bootloader_generic(uint8_t cmd, uint8_t *data, uint8_t data_size);
int cmd_request_imu_telem(IMU *imu);
int cmd_request_gps_telem(GPS *gps);
int cmd_request_barometer_telem(BAROMETER *barometer);
int cmd_request_power_telem(POWER *power);

#endif