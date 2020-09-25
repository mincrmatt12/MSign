#include "serial.h"
#include "util.h"
#include "debug.h"
#include "wifitime.h"

#include <algorithm>
#include <ctime>
#include <esp_log.h>
#include <sys/time.h>

serial::SerialInterface serial::interface;

#define STASK_BIT_PACKET 1
#define STASK_BIT_REQUEST 2

static const char * TAG = "servicer";

void serial::SerialInterface::run() {
	// Initialize queue
	requests = xQueueCreate(4, sizeof(Request));
	xTaskNotifyStateClear(NULL);

	srv_task = xTaskGetCurrentTaskHandle();
	// Initialize the protocol
	init_hw();

	ESP_LOGI(TAG, "Initialized servicer HW");

	// Do a handshake
	while (true) {
		// Wait for a packet
		while (wait_for_event() == EventQueue) {;}
		// Check what packet we received
		auto cmd = rx_buf[2];
		continue_rx();
		if (cmd != slots::protocol::HANDSHAKE_INIT) {
			ESP_LOGW(TAG, "Invalid handshake init");
			continue;
		}

		// Send out a handshake_ok
		uint8_t buf_reply[3] = {
			0xa6,
			0x00,
			slots::protocol::HANDSHAKE_RESP
		};

		send_pkt(buf_reply);

		// Wait for another packet
		while (wait_for_event() == EventQueue) {;}
		// Check what packet we received
		cmd = rx_buf[2];
		continue_rx();
		if (cmd != slots::protocol::HANDSHAKE_OK) {
			ESP_LOGW(TAG, "Invalid handshake response");
			continue;
		}
		break;
	}

	ESP_LOGI(TAG, "Connected to STM32");

	while (true) {
		auto event = wait_for_event(active_request.type == Request::TypeEmpty ? pdMS_TO_TICKS(10000) : pdMS_TO_TICKS(1000));
		// this should be a switch...
		if (event == EventPacket || event == EventAll) {
			if (active_request.type == Request::TypeEmpty)
				process_packet();
			else {
				if (!packet_request())
					process_packet();
			}

			if (event == EventAll) goto check_request;
		}
		else if (event == EventQueue) {
check_request:
			if (xQueueReceive(requests, &active_request, 0))
				start_request();
		}
		else {
			if (active_request.type != Request::TypeEmpty) {
				loop_request();
			}
			else {
				// TODO: ping stuff
			}
		}

		if (should_check_request) {
			should_check_request = false;
			goto check_request;
		}
	}
}

void serial::SerialInterface::reset() {
	Request r;
	r.type = Request::TypeReset;
	xQueueSend(requests, &r, portMAX_DELAY);
}

void serial::SerialInterface::process_packet() {
	switch (rx_buf[2]) {
		case slots::protocol::PING:
			{
				ESP_LOGD(TAG, "got ping");
				continue_rx();

				uint8_t resp[3] = {
					0xa6,
					0x00,
					slots::protocol::PONG
				};

				send_pkt(resp);
			}
			break;
		case slots::protocol::QUERY_TIME:
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
					continue_rx();
					resp[3] = 0;
					memcpy(resp + 4, &millis, sizeof(millis));
				}
				else {
					continue_rx();
					resp[3] = (uint8_t)slots::protocol::TimeStatus::NotSet;
				}

				send_pkt(resp);
			}
			break;
		case slots::protocol::DATA_TEMP:
			{
				// When we receive a temperature message we have to do one of a few things:
				// If we've never seen this block:
				// 	- Create an empty placeholder
				// If we have this block:
				//  Set the temperature
				// 	If we're moving from cold -> hot,warm:
				// 	 If there are remote blocks:
				// 	  Mark all the blocks as flush
				//   Queue a block update: blocks are kept updated in terms of dirty but only flushed
				//                         if temperature is hot enough
				uint16_t slotid; memcpy(&slotid, rx_buf + 3, 2);
				uint8_t  reqtemp = rx_buf[5];
				continue_rx();

				ESP_LOGD(TAG, "Setting %03x to %d", slotid, reqtemp);

				// Check if this is a new block
				if (!arena.contains(slotid)) {
					// If it is, just put a placeholder
					if (!arena.add_block(slotid, bheap::Block::LocationCanonical, 0)) {
						ESP_LOGW(TAG, "Ran out of space trying to allocate placeholder for %03x", slotid);
						// Ignore and continue
						break;
					}
					ESP_LOGD(TAG, "Added empty placeholder");
					arena.set_temperature(slotid, reqtemp);
				}
				else {
					// Update the temperature
					arena.set_temperature(slotid, reqtemp);
					// Check if we need to flush "the rest" of the block.
					if (reqtemp & 0b10) {
						bool needs_update = false;

						if (std::any_of(arena.begin(slotid), arena.end(slotid), [](const auto &x){return x.location == bheap::Block::LocationRemote;}))
							needs_update = true;
						// If we need to send anything, send it
						if (needs_update) {
							for (auto it = arena.begin(slotid); it != arena.end(slotid); ++it) {
								if (it->location == bheap::Block::LocationCanonical && !(it->flags & bheap::Block::FlagFlush)) {
									it->flags |= bheap::Block::FlagFlush;
								}
							}
							ESP_LOGD(TAG, "Queueing block update since there were unflushed remote blocks");
							update_blocks();
						}
					}
				}

				// Send a response ack
				uint8_t response[6] = {
					0xa6,
					0x3,
					slots::protocol::ACK_DATA_TEMP,
					0,
					0,
					reqtemp
				};
				memcpy(response + 3, &slotid, 2);
				send_pkt(response);
			}
			break;
		default:
			ESP_LOGW(TAG, "Unknown packet type %02x", rx_buf[2]);
			continue_rx();
			break;
	}
}

void serial::SerialInterface::start_request() {
	ESP_LOGE(TAG, "Not implemented: ignoring request");
	finish_request();
}

bool serial::SerialInterface::packet_request() {
	ESP_LOGE(TAG, "Not implemented: ignoring request packet");
	return false;
}

void serial::SerialInterface::loop_request() {
	ESP_LOGE(TAG, "Not implemented: ignoring request loop");
	finish_request();
}

void serial::SerialInterface::finish_request() {
	active_request.type = Request::TypeEmpty;
	should_check_request = true;
}

serial::SerialInterface::Event serial::SerialInterface::wait_for_event(TickType_t t) {
	uint32_t f;
rewait:
	if (xTaskNotifyWait(0, STASK_BIT_PACKET | STASK_BIT_REQUEST, &f, t) == pdFAIL) {
		return EventTimeout;
	}
	if ((f & (STASK_BIT_PACKET | STASK_BIT_REQUEST)) == (STASK_BIT_PACKET | STASK_BIT_REQUEST)) return EventAll;
	if (f & STASK_BIT_PACKET) return EventPacket;
	if (f & STASK_BIT_REQUEST) return EventQueue;
	goto rewait;
}

void serial::SerialInterface::on_pkt() {
	// Send a notification
	xTaskNotify(srv_task, STASK_BIT_PACKET, eSetBits);
}

void serial::SerialInterface::update_blocks() {
	// TODO:
	ESP_LOGW(TAG, "Not implemented: update_blocks()");
}
