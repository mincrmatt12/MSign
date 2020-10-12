#ifndef ESP_SYSTEM_FAKE
#define ESP_SYSTEM_FAKE

#define heap_caps_get_free_size(x) 10000

#include <stdlib.h>

static void esp_restart() {
	exit(1);
}

#define esp_random random
#define IRAM_ATTR

#endif
