#include "serial.h"
#include "config.h"
#include "webui.h"
#include "wifitime.h"
#include "upd.h"
#include "sd.h"
#include "config.h"
#include "grabber/grab.h"

#include <esp_log.h>
#include <esp_system.h>
#include <FreeRTOS.h>
#include <task.h>

const static char * TAG = "app_main";

#ifdef SIM
#define STACK_MULT 8
#else
#define STACK_MULT 1
#endif

extern "C" void app_main() {
	ESP_LOGI(TAG, "Starting MSign...");

	// Try and start the SD layer
	switch (sd::init()) {
		case sd::InitStatus::NoCard:
			ESP_LOGE(TAG, "=============================");
			ESP_LOGE(TAG, "No card is inserted! Halting!");
			ESP_LOGE(TAG, "=============================");
			return;
		case sd::InitStatus::UnusableCard:
		case sd::InitStatus::UnkErr:
			ESP_LOGE(TAG, "Unusable/unknown error while initializing the card.");
			return;
		case sd::InitStatus::FormatError:
			ESP_LOGE(TAG, "No filesystem on card");
			// TODO: maybe indicate these failures to the STM somehow?
			return;
		default:
			break;
	}

	// check for updates
	switch (upd::needed()) {
		case upd::FULL_SYSTEM:
			upd::update_system();
			ESP_LOGW(TAG, "somehow returned from update_system? restarting anyways.");
			esp_restart();
			break;
		default:
			break;
	}

	// Install logger (disabled for now)
	// sd::install_log();

	serial::interface.init();

	// Create event handle early
	wifi::events = xEventGroupCreate();

	// Start up the servicer
	if (xTaskCreate([](void *ptr){
		((serial::SerialInterface *)ptr)->run();
	}, "srv", 3072 * STACK_MULT, &serial::interface, 9, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create srv");
		return;
	}

	// Load the config from the SD card
	if (!config::parse_config_from_sd()) {
		return;
	}

	if (!wifi::init()) {
		ESP_LOGE(TAG, "Not starting data services due to no WIFI.");
		// TODO: make this setup softap mode for netcfg
	}

	// Start the grabber
	if (xTaskCreate(grabber::run, "grab", 7680 * STACK_MULT, nullptr, 6, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create grab");
		return;
	}

	if (xTaskCreate(webui::run, "webUI", 4096 * STACK_MULT, nullptr,  5, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create webui; running without it");
	}

	ESP_LOGI(TAG, "Created tasks (1)");
	ESP_LOGI(TAG, "Free heap available is %d", (int)heap_caps_get_free_size(pvMALLOC_DRAM));
}
