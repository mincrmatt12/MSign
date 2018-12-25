#include "HardwareSerial.h"
#include "serial.h"

serial::SerialInterface serial::interface;

uint16_t serial::search_for(uint16_t val, uint16_t array[256]) {
	for (uint8_t offset = 0; offset < 128; ++offset) {
		if (array[offset] == val) return offset;
		if (array[256-offset-1] == val) return 256-offset-1;
	}
	return ~0;
}

void serial::SerialInterface::ensure_handshake() {
	// wait for the incoming HANDSHAKE_INIT command
	uint8_t buf[3];
try_again:
	Serial.readBytes(buf, 3);

	if (buf[0] != 0xa5 || buf[1] != 0x00 || buf[2] != serial::HANDSHAKE_INIT) goto try_again;
	buf[0] = 0xa6;
	buf[2] = serial::HANDSHAKE_RESP;

	Serial.write(buf, 3);
	Serial.readBytes(buf, 3);

	if (buf[0] != 0xa5 || buf[1] != 0x00 || buf[2] != serial::HANDSHAKE_OK) goto try_again;

	Serial1.println(F("Connected to STM32"));
}

void serial::SerialInterface::loop() {
	// Check if there is incoming data for a header.
	if (waiting_size != 0) {
		// we are waiting for incoming data
		if (Serial.available() >= waiting_size) {
			uint8_t buf[waiting_size];
			Serial.readBytes(buf, waiting_size);
			handle_command(static_cast<serial::Command>(pending_command), waiting_size, buf);
			waiting_size = 0;
		}
	}
	else if (Serial.available() >= 3) {
		uint8_t buf[3];
		Serial.readBytes(buf, 3);
		if (buf[0] == 0xa6) {
			Serial1.println(F("STM32 is having an identity crisis, it thinks it's an ESP8266"));
		}
		else if (buf[0] != 0xa5) {
			Serial1.println(F("STM32 is drunk; sent invalid header."));
		}
		else if (buf[1] != 0x00) {
			pending_command = buf[2];
			waiting_size = buf[1];
		}
		else {
			handle_command(static_cast<serial::Command>(buf[2]), 0, nullptr);
		}
	}
}

void serial::SerialInterface::handle_command(serial::Command cmd, uint8_t size, uint8_t *buf) {
	switch (cmd) {
		case HANDSHAKE_INIT:
		case HANDSHAKE_RESP:
		case HANDSHAKE_OK:
			{
				Serial1.println(F("Encountered handshake commands in main loop."));
			}
			break;
		case OPEN_CONN:
			{
				// open conn
				if (size < 4) goto size_error;

				if (buf[0] == 0x00) {
					// polled enable.
					slots_continuous[buf[1]] = *(uint16_t *)(buf + 2);
				}
				else {
					// else cont.
					slots_polled[buf[1]] = *(uint16_t *)(buf + 2);
				}

				uint8_t buf_reply[4] = {
					0xa6,
					0x01,
					ACK_OPEN_CONN,
					buf[1]
				};

				Serial.write(buf_reply, 4);

				if (buf[0] == 0x00) {
					update_polled_data(buf[1]);
				}
				else {
					update_open_handlers(slots_continuous[buf[1]]);
				}
			}
			break;
		case CLOSE_CONN:
			{
				// close the connection
				if (size < 1) goto size_error;

				slots_continuous[buf[1]] = 0x00;
				slots_polled[buf[1]] = 0x00;

				uint8_t buf_reply[4] = {
					0xa6,
					0x01,
					ACK_CLOSE_CONN,
					buf[1]
				};

				Serial.write(buf_reply, 4);
			}
			break;
		case NEW_DATA:
			{
				if (size < 1) goto size_error;

				update_polled_data(buf[0]);
			}
			break;
		default:
			{
				Serial1.print(F("Got an invalid command or command that should only come from us.\n"));
			}
	}

	return;

size_error:
	Serial1.printf("Size error on cmd %02x", cmd);
	return;
}


void serial::SerialInterface::send_data_to(uint8_t slot_id, const uint8_t * buffer, uint8_t length) {
	uint8_t buf[length + 4];
	buf[0] = 0xa6;
	buf[1] = length + 1;
	buf[2] = serial::SLOT_DATA;
	buf[3] = slot_id;
	memcpy(buf + 4, buffer, length);
	
	Serial.write(buf, length + 4);
}

void serial::SerialInterface::update_polled_data(uint8_t slot_id) {
	uint16_t data_id = slots_polled[slot_id];
	uint8_t buf[16];
	uint8_t length;

	for (uint8_t i = 0; i < number_of_handlers; ++i) {
		if (handlers[i](data_id, buf, length)) goto ok;
	}

	return;
ok:
	send_data_to(slot_id, buf, length);
}

void serial::SerialInterface::register_handler(const QueryHandler handler) {
	if (number_of_handlers == 8) {
		Serial1.println(F("Too many query handlers...\n"));
		return;
	}
	handlers[number_of_handlers++] = handler;
}

void serial::SerialInterface::update_data(uint16_t data_id, const uint8_t * buffer, uint8_t length) {
	uint16_t pos = search_for(data_id, this->slots_continuous);
	if (pos != (uint16_t)(~0)) {
		send_data_to(pos, buffer, length);
	}
}

void serial::SerialInterface::update_open_handlers(uint16_t data_id) {
	for (uint8_t i = 0; i < number_of_o_handlers; ++i) {
		o_handlers[i](data_id);
	}
}
