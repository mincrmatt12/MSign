#pragma once

#include <FreeRTOS.h>
#include <stdint.h>
#include "../common/slots.h"
#include "../serial.h"
#include "esp_log.h"

namespace octoprint {
	struct GcodeProvider {
		// Go back to the beginning of the stream
		virtual bool restart() = 0;

		// Read some gcode into the provided buffer. Return the number of bytes read. If the return is <=0, an error is logged.
		virtual int read(uint8_t *buf, size_t maxlen) = 0;

		// If true, there was a download phase before using this provider, so the parser should report as
		// using progress from 25-100 instead of 0-100.
		virtual bool has_download_phase() const { return true; }

		virtual size_t gcode_size() const = 0;

		// Returns true if at eof.
		inline bool is_done() const { return _is_done; }
	protected:
		~GcodeProvider() = default;

		bool _is_done = false;

		GcodeProvider& operator=(const GcodeProvider&) = default;
		GcodeProvider& operator=(GcodeProvider&&) = default;
	};

	// Processes the gcode and writes out the helper files.
	bool process_gcode(GcodeProvider &gcode, uint16_t &layer_count_out);

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
		void set_download_phase_flag(bool flag) {
			temp_bitinfo.file_process_has_download_phase = flag;
			last_sent = xTaskGetTickCount();
			serial::interface.update_slot_nosync(slots::PRINTER_BITMAP_INFO, temp_bitinfo);
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
