#include "protocol.h"

#include <string.h>
#include <esp_system.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <iostream>
#include <sys/fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <esp_log.h>
#include <unistd.h>

const static char *TAG = "protocol";

void uart_write_bytes(const uint8_t *data, size_t length) {
	while (length--) {
		putc(*data++, stdout);
	}
	fflush(stdout);
}

void uart_read_bytes(uint8_t *data, size_t length) {
	size_t bytes_read = 0;
	while (bytes_read < length) {
		int amount = read(0, data, length - bytes_read);
		if (amount < 0) {
			if (errno != EWOULDBLOCK) {
				perror("failed to read stdin");
				exit(1);
			}
		}
		else {
			bytes_read += amount;
			data += amount;
		}
		vTaskDelay(1);
	}
}

void protocol::ProtocolImpl::init_hw() {
	memset(rx_buf, 0, sizeof rx_buf);

	std::cout.tie(0);

	// Setup fake UART with pipes
	termios term;
    tcgetattr(0, &term);
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    tcsetattr(0, TCSANOW, &term);

	fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

	BaseType_t res;
	if ((res = xTaskCreate([](void *ptr){
		((protocol::ProtocolImpl *)ptr)->rx_task();
	}, "rxST", configMINIMAL_STACK_SIZE, this, 13, &rx_thread)) != pdPASS) {
		ESP_LOGE(TAG, "failed to create rxST %ld", res);
	}
}

void protocol::ProtocolImpl::send_pkt(const void *pkt) {
	uart_write_bytes((const uint8_t *)pkt, reinterpret_cast<const uint8_t *>(pkt)[1] + 3);
}

void protocol::ProtocolImpl::continue_rx() {
	xTaskNotify(rx_thread, 0x5555, eSetValueWithOverwrite);
}

void protocol::ProtocolImpl::rx_task() {
	while (true) {
		// Wait for a sync byte
		uart_read_bytes(rx_buf, 3);
		while (rx_buf[0] != 0xa5) {
			rx_buf[0] = rx_buf[1];
			rx_buf[1] = rx_buf[2];
			uart_read_bytes(rx_buf + 2, 1);
		}
		// Read the rest of the packet
		uart_read_bytes(rx_buf + 3, rx_buf[1]);
		// Signal packet completed
		ESP_LOGD(TAG, "got pkt");
		on_pkt();
		// Wait for sync
		uint32_t f = 0;
		while (f != 0x5555) {
			xTaskNotifyWait(0, 0xffff'ffff, &f, portMAX_DELAY);
		}
	}
}

unsigned protocol::ProtocolImpl::get_processing_delay() {
	return 0;
}
