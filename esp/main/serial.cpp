#include "serial.h"
#include "esp_system.h"
#include "sd.h"
#include "wifitime.h"

#include <ctime>
#include <esp_log.h>
#include <sys/time.h>

#include "grabber/grab.h"
#include "serial.cfg.h"

#include <ff.h>

#include <crashlog_storage.h>

serial::SerialInterface serial::interface;

static const char * const TAG = "servicer";

// Data update manager: handles requests for data separately
#ifndef SIM
#include <esp_attr.h>
__NOINIT_ATTR
#endif
crashlogs::CrashBuffer<serial::DataUpdateManager> dum;

void serial::SerialInterface::process_packet() {
	switch (rx_buf[2]) {
		using namespace slots::protocol;

		case PING:
			{
				ESP_LOGD(TAG, "got ping");

				uint8_t resp[3] = {
					0xa6,
					0x00,
					slots::protocol::PONG
				};

				send_pkt(resp);
			}

			return;

		case REFRESH_GRABBER:
			{
				ESP_LOGD(TAG, "Refreshing grabber");

				grabber::refresh((slots::protocol::GrabberID)rx_buf[3]);
			}
			break;

		case QUERY_TIME:
			{
				ESP_LOGD(TAG, "got timereq");
				uint8_t resp[12] = {
					0xa6,
					9,
					slots::protocol::QUERY_TIME,
					0,
					0, 0, 0, 0, 0, 0, 0, 0
				};
				
				// Check if the time is set
				if (xEventGroupGetBits(wifi::events) & wifi::TimeSynced) {
					uint64_t millis = wifi::get_localtime();
					millis -= get_processing_delay();
					resp[3] = 0;
					memcpy(resp + 4, &millis, sizeof(millis));
				}
				else {
					resp[3] = (uint8_t)slots::protocol::TimeStatus::NotSet;
				}

				send_pkt(resp);
			}

			return;
		case SLEEP_ENABLE:
			{
				bool entering = rx_pkt.data()[0];
				
				if (entering) {
					ESP_LOGI(TAG, "Entering sleep mode... (logs will quieten)");

					// ... logs are now shutupped
					
					in_sleep_mode = true;
				}
				else {
					in_sleep_mode = false;

					// Exit sleep mode
					ESP_LOGI(TAG, "Exited sleep mode");

					// refresh all grabbers
					grabber::refresh(slots::protocol::GrabberID::ALL);
				}
			}
			return;
		case DATA_TEMP:
			{
				DataUpdateRequest dur;
				dur.type = DataUpdateRequest::TypeSetTemp;
				dur.d_temp.slotid = rx_pkt.at<uint16_t>(0);
				dur.d_temp.newtemperature = rx_buf[5];
				// Prevent clogging up ongoing packet routing to the dupm if it's stuck
				// processing a _different_ packet
				if (!dum->queue_request(dur, 0)) {
					ESP_LOGW(TAG, "dum busy?");
				}
			}

			return;

		case DATA_REQUEST:
			{
				DataUpdateRequest dur;
				dur.type = DataUpdateRequest::TypeMarkDirty;
				dur.d_dirty.slotid = rx_pkt.at<uint16_t>(0); 
				dur.d_dirty.offset = rx_pkt.at<uint16_t>(2); 
				dur.d_dirty.size = rx_pkt.at<uint16_t>(4);
				// Prevent clogging up ongoing packet routing to the dupm if it's stuck
				// processing a _different_ packet
				if (!dum->queue_request(dur, 0)) {
					ESP_LOGW(TAG, "dum busy?");
				}
			}
			
			return;

		case RESET:
			{
				ESP_LOGW(TAG, "Got reset from STM; resetting.");
				esp_restart();
			}

		case slots::protocol::HANDSHAKE_INIT:
			{
				// TODO: maybe make this just mark all things in the dum dirty and only reset if data is lost?
				ESP_LOGW(TAG, "Got handshake init in main loop; resetting");
				esp_restart();
			}
			break;

		case UI_GET_CALIBRATION:
			{
				FIL f;
				if (f_open(&f, "/adc.bin", FA_READ) != FR_OK) {
					slots::PacketWrapper<1> resp;
					resp.init(UI_GET_CALIBRATION);
					resp.put<AdcCalibrationResult>(adc_enabled ? AdcCalibrationResult::MISSING : AdcCalibrationResult::MISSING_IGNORE, 0);
					send_pkt(resp);
				}
				else {
					slots::PacketWrapper<1 + sizeof(AdcCalibration)> resp;
					resp.init(UI_GET_CALIBRATION);
					UINT br;
					if (f_read(&f, resp.data() + 1, sizeof(AdcCalibration), &br) != FR_OK || br != sizeof(AdcCalibration)) {
						resp.put<AdcCalibrationResult>(adc_enabled ? AdcCalibrationResult::MISSING : AdcCalibrationResult::MISSING_IGNORE, 0);
					}
					else {
						resp.put<AdcCalibrationResult>(AdcCalibrationResult::OK, 0);
					}
					f_close(&f);
					send_pkt(resp);
				}
			}
			break;

		case UI_SAVE_CALIBRATION:
			{
				FIL f;
				if (f_open(&f, "/adc.bin", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
					break;
				}
				UINT bw;
				if (f_write(&f, rx_pkt.data(), sizeof(AdcCalibration), &bw) != FR_OK) {
					f_close(&f);
					break;
				}
				f_close(&f);
			}
			{
				uint8_t resp[3] {
					0xa6,
					0,
					slots::protocol::UI_SAVE_CALIBRATION
				};
				send_pkt(resp);
			}
			break;
		
		case ACK_DATA_TEMP: // Ignore acks for the set_temp calls we make
			break;

		default:
			ESP_LOGW(TAG, "Unknown packet type %02x", rx_buf[2]);
			break;
	}
}

void serial::SerialInterface::run() {
	srv_task = xTaskGetCurrentTaskHandle();
	// Initialize the protocol
	init_hw();

	ESP_LOGI(TAG, "Initialized servicer HW");

	{
		bool state = false;
		// Do a handshake
		while (true) {
			wait_for_packet();

			// Is this the correct packet?
			auto cmd = rx_buf[2];

			if (!state) {
				if (cmd != slots::protocol::HANDSHAKE_INIT) {
					ESP_LOGW(TAG, "Invalid handshake init");
					continue;
				}
				state = true;

				// Send out a handshake_ok
				uint8_t buf_reply[3] = {
					0xa6,
					0x00,
					slots::protocol::HANDSHAKE_RESP
				};

				send_pkt(buf_reply);
				continue;
			}
			else {
				if (cmd == slots::protocol::HANDSHAKE_INIT) continue;
				if (cmd != slots::protocol::HANDSHAKE_OK) {
					state = false;
					ESP_LOGW(TAG, "Invalid handshake response");
					continue;
				}

				break;
			}
		}
	}

	ESP_LOGI(TAG, "Connected to STM32");
	xEventGroupSetBits(wifi::events, wifi::StmConnected);

#ifdef SIM
#define STACK_MULT 8
#else
#define STACK_MULT 1
#endif
	// Init data update manager task
	if (xTaskCreate([](void *ptr){
		((serial::DataUpdateManager *)ptr)->run();
	}, "dupm", 2304 * STACK_MULT, &dum, 9, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create dupm");
		return;
	}
#undef STACK_MULT

	// Main servicer loop
	while (true) {
		// Perform bookkeeping tasks
		send_pings();

		// Wait for a packet for a bit.
		if (!wait_for_packet(portMAX_DELAY)) {
			ESP_LOGW(TAG, "bad packet");
		}

		// Try to process with the data update manager
		if (dum->is_packet_in_pending(rx_buf)) {
			if (dum->process_input_packet(rx_buf)) continue;
		}

		// Otherwise, process here.
		process_packet();
	}
}

void serial::SerialInterface::send_pings() {
	// TODO
}

void serial::SerialInterface::reset() {
	// End thine suffering as doest thou commit one holy reset  (TODO: handle advanced resets)
	uint8_t pkt[3] = {
		0xa6,
		0x00,
		slots::protocol::RESET
	};

	send_pkt(pkt);
	ESP_LOGE(TAG, "The system is going down for reset.");
	vTaskDelay(pdMS_TO_TICKS(100));
	esp_restart();
}

void serial::SerialInterface::set_sleep_mode(bool mode) {
	uint8_t pkt[4] = {
		0xa6,
		0x01,
		slots::protocol::SLEEP_ENABLE,
		static_cast<uint8_t>(mode ? 0x1 : 0x0)
	};

	send_pkt(pkt);
	if (in_sleep_mode && !mode) grabber::refresh(slots::protocol::GrabberID::ALL);
	in_sleep_mode = mode;
}

void serial::SerialInterface::update_slot_raw(uint16_t slotid, const void *ptr, size_t length, bool should_sync, bool should_mark_dirty) {
	allocate_slot_size(slotid, length);
	update_slot_partial(slotid, 0, ptr, length, should_sync, should_mark_dirty);
}

void serial::SerialInterface::allocate_slot_size(uint16_t slotid, size_t size) {
	DataUpdateRequest dur;
	dur.type = DataUpdateRequest::TypeChangeSize;
	dur.d_chsize.slotid = slotid;
	dur.d_chsize.size   = size;
	dum->queue_request(dur);
}

size_t serial::SerialInterface::current_slot_size(uint16_t slotid) {
	size_t result = 0u;

	DataUpdateRequest dur;
	dur.type = DataUpdateRequest::TypeGetSize;
	dur.d_getsz.slotid = slotid;
	dur.d_getsz.cursize_out = &result;

	dum->queue_request(dur);
	sync();
	return result;
}

void serial::SerialInterface::trigger_slot_update(uint16_t slotid) {
	DataUpdateRequest dur;
	dur.type = DataUpdateRequest::TypeTriggerUpdate;
	dur.d_trigger.slotid = slotid;
	dum->queue_request(dur);
}

void serial::SerialInterface::update_slot_partial(uint16_t slotid, uint16_t offset, const void * ptr, size_t length, bool should_sync, bool should_mark_dirty) {
	if (length > 11) {
		{
			DataUpdateRequest dur;
			dur.type = should_mark_dirty ? DataUpdateRequest::TypePatch : DataUpdateRequest::TypePatchWithoutMarkDirty;
			dur.d_patch.data = ptr;
			dur.d_patch.length = length;
			dur.d_patch.offset = offset;
			dur.d_patch.slotid = slotid;
			dum->queue_request(dur);
		}

		if (should_sync) {
			sync();
		}
	}
	else {
		DataUpdateRequest dur;
		dur.type = should_mark_dirty ? DataUpdateRequest::TypeInlinePatch : DataUpdateRequest::TypeInlinePatchWithoutMarkDirty;
		memcpy(dur.d_inline_patch.data, ptr, length);
		dur.d_inline_patch.slotid = slotid;
		dur.d_inline_patch.length = length;
		dur.d_inline_patch.offset = offset;
		dum->queue_request(dur);
	}
}

void serial::SerialInterface::sync() {
	{
		DataUpdateRequest dur;
		dur.type = DataUpdateRequest::TypeSync;
		dur.d_sync.with = xTaskGetCurrentTaskHandle();
		dur.d_sync.by = portMAX_DELAY; // todo
		if (!dum->queue_request(dur)) {
			ESP_LOGW(TAG, "Failed to queue sync request");
			vTaskDelay(1000);
			return;
		}
	}
	xTaskNotifyWait(0, 0xffff'ffff, nullptr, portMAX_DELAY);
}

void serial::SerialInterface::init() {
	dum.init_underlying();
	dum->init();
}

void serial::process_stored_crashlogs() {
	static const char * const TAG = "sdcrash";

	if (const char * log_data = dum.saved_log()) {
		size_t log_size = dum.saved_log_length();

		sd::refresh_log_dir("crash");
		f_unlink("/log/crash.0");

		ESP_LOGE(TAG, "Found old crash log in memory, writing to SD");

		FIL cra{}; 
		if (f_open(&cra, "/log/crash.0", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
			ESP_LOGE(TAG, "Failed to open new crash log");
			return;
		}

		UINT bw;
		f_write(&cra, log_data, log_size, &bw);

		f_sync(&cra);
		f_close(&cra);
	}
}

#if !defined(SIM) && defined(CONFIG_ENABLE_CRASHLOGS)

#include <crashlog_writer.h>

extern "C" void esp_custom_panic_handler(void *frame, int wdt) {
	crashlogs::write_panic_frame(dum, frame, wdt);
}

#endif
