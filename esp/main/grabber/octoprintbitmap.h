#pragma once

#include <FreeRTOS.h>
#include <stdint.h>
#include "../common/slots.h"
#include "../serial.h"
#include "esp_log.h"

namespace octoprint {
	// Processes the gcode and writes out the helper files.
	bool process_gcode(uint16_t &layer_count_out);

	constexpr inline uint32_t RANDOM_FILEPOS = ~0u;

	// Sends out the correct layer bitmap
	void send_bitmap_for(uint32_t filepos);

	struct GcodeParseProgressTracker {
		void fail() {
			update(temp_bitinfo.PROCESSED_FAIL);
		}
		void update(int8_t progress) {
			if (progress == temp_bitinfo.file_process_percent)
				return;
			temp_bitinfo.file_process_percent = progress;
			commit();
		}
		~GcodeParseProgressTracker() {
			serial::interface.update_slot_nosync(slots::PRINTER_BITMAP_INFO, temp_bitinfo);
			serial::interface.sync();
		}
	private:
		TickType_t last_sent = xTaskGetTickCount();
		slots::PrinterBitmapInfo temp_bitinfo{};

		void commit() {
			if (xTaskGetTickCount() - last_sent < pdMS_TO_TICKS(25))
				return;
			ESP_LOGD("octoprintbitmap", "parse progress %d", (int)temp_bitinfo.file_process_percent);
			last_sent = xTaskGetTickCount();
			serial::interface.update_slot_nosync(slots::PRINTER_BITMAP_INFO, temp_bitinfo);
		}
	};
}
