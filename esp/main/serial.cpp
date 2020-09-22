#include "serial.h"
#include "util.h"
#include "debug.h"

#include <esp_log.h>

serial::SerialInterface serial::interface;

#define STASK_BIT_PACKET 1
#define STASK_BIT_REQUEST 2

const char * TAG = "servicer";

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
				ESP_LOGW(TAG, "ping pkt");
				continue_rx();

				uint8_t resp[3] = {
					0xa6,
					0x00,
					slots::protocol::PONG
				};

				send_pkt(resp);
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
