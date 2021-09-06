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
	BOOTCMD_UPDATE = 0x01,
	BOOTCMD_NEW_LOADER = 0x02
};
typedef enum _bootcmd_t bootcmd_t;

bool bootcmd_did_just_update();
bootcmd_t bootcmd_get_cmd();

uint32_t bootcmd_update_size();
void     bootcmd_request_update(uint32_t size);
void 	 bootcmd_service_update();
void     bootcmd_set_silent(bool on);
bool     bootcmd_get_silent();

const char * bootcmd_get_bl_revision();

// BOOTCMD is implemented using the RTC backup registers, similar to the ESP8266's eboot.
//
// Register Map
// 00 - bootcmd
// 01 - did just update flag, cleared by application
// 02 - requested update size
// 03 - bootloader revision string (max 4 chars)
// 04 - is silent/sleep mode (do updates/reboots without turning on the screen)

#ifdef __cplusplus
};
#endif

#endif
