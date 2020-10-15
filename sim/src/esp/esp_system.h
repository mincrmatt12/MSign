#ifndef ESP_SYSTEM_FAKE
#define ESP_SYSTEM_FAKE

#define heap_caps_get_free_size(x) 10000
#define esp_get_free_heap_size() 10000

#include <stdlib.h>
#include <stddef.h>

static void esp_restart() {
	exit(1);
}

static void esp_fill_random(void *buf, size_t len) {}

#define esp_random random
#define IRAM_ATTR

#endif
