#include "HardwareSerial.h"
#include "serial.h"

serial::SerialInterface serial::interface;

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

	Serial1.println("Connected to STM32");
}
