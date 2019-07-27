#ifndef BOOTCMD_H
#define BOOTCMD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void bootcmd_init();

enum _bootcmd_t {
	BOOTCMD_RUN = 0x00ull,
	BOOTCMD_UPDATE = 0x01
};
typedef enum _bootcmd_t bootcmd_t;

bool bootcmd_did_just_update();
bootcmd_t bootcmd_get_cmd();

uint32_t bootcmd_update_size();
void     bootcmd_request_update(uint32_t size);
void 	 bootcmd_service_update();

// BOOTCMD is implemented using the RTC backup registers, similar to the ESP8266's eboot.
//
// Register Map
// 00 - bootcmd
// 01 - did just update flag, cleared by application
// 02 - requested update size

#ifdef __cplusplus
};
#endif

#endif
