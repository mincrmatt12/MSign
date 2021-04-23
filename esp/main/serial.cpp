#include "serial.h"
#include "esp_system.h"
#include "wifitime.h"

#include <algorithm>
#include <ctime>
#include <esp_log.h>
#include <sys/time.h>

serial::SerialInterface serial::interface;

#define STASK_BIT_PACKET 1
#define STASK_BIT_REQUEST 2

static const char * TAG = "servicer";
static const uint32_t max_single_update_size = 1024;

namespace serial::st {
	struct UpdateSizeTask final : public serial::SerialSubtask {
		const static inline int TYPE = 1;

		UpdateSizeTask(uint16_t slotid, uint16_t newsize) : new_size(newsize) {
			this->slotid = slotid;
		}

		void start() override {
		};

	protected:
		int subtask_type() const override {
			return TYPE;
		}

		uint16_t new_size;
	};
};

void serial::SerialInterface::run() {
	// Initialize queue
	events = xQueueCreate(4, sizeof(SerialEvent));
	xTaskNotifyStateClear(NULL);
	request_overflow = xQueueCreate(8, sizeof(SerialEvent));

	srv_task = xTaskGetCurrentTaskHandle();
	// Initialize the protocol
	init_hw();

	ESP_LOGI(TAG, "Initialized servicer HW");

	// Do a handshake
	while (true) {
	}

	ESP_LOGI(TAG, "Connected to STM32");
}

void serial::SerialInterface::reset() {
	SerialEvent r;
	r.event_type = SerialEvent::EventTypeRequest;
	r.event_subtype = SerialEvent::EventSubtypeSystemReset;
	xQueueSend(events, &r, portMAX_DELAY);
}

void serial::SerialInterface::update_slot(uint16_t slotid, const void *ptr, size_t length) {
}

void serial::SerialInterface::allocate_slot_size(uint16_t slotid, size_t size) {
}

void serial::SerialInterface::update_slot_partial(uint16_t slotid, uint16_t offset, const void * ptr, size_t length) {
}
