#ifndef BOOTCMD_H
#define BOOTCMD_H

#include <stdint.h>

#ifdef __cplusplus
namespace bootcmd {
extern "C" {
#endif

enum bootcmd_cmd_t {
	BOOTCMD_RUN = 0x00,
	BOOTCMD_UPDATE = 0x01
};

typedef struct _bootcmd {
	uint8_t signature[2];
	uint8_t cmd;
	
	uint32_t size;

#ifdef __cplusplus
	private:
#endif

	uint8_t is_safe;
	uint32_t current_size;
	
} bootcmd_t;

#define BCMD_BASE 0x080FC000
#define BCMD ((__IO bootcmd_t *)BCMD_BASE)

#ifdef __cplusplus
};
}
#endif

#endif
