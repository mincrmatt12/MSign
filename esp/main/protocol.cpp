#include "protocol.h"

#include <string.h>
#include <uart.h>
#include <esp_system.h>

const static char *TAG = "protocol";

void protocol::ProtocolImpl::init_hw() {
	memset(rx_buf, 0, sizeof rx_buf);

	// Initialize UART0
	uart_config_t cfg;
	cfg.baud_rate = 115200;
	cfg.data_bits = UART_DATA_8_BITS;
	cfg.stop_bits = UART_STOP_BITS_1;
	cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	cfg.parity = UART_PARITY_EVEN;
	uart_param_config(UART_NUM_0, &cfg);

	// Setup the driver
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
}

void protocol::ProtocolImpl::send_pkt(const void *pkt) {
	uart_write_bytes(UART_NUM_0, (const char *)pkt, reinterpret_cast<const uint8_t *>(pkt)[1] + 3);
}

bool protocol::ProtocolImpl::wait_for_packet(TickType_t to) {
	// Wait for a sync byte
	if (!uart_read_bytes(UART_NUM_0, rx_buf, 3, to)) return false;
	while (rx_buf[0] != 0xa5) {
		rx_buf[0] = rx_buf[1];
		rx_buf[1] = rx_buf[2];
		if (!uart_read_bytes(UART_NUM_0, rx_buf + 2, 1, to)) return false;
	}
	// Set last received at
	last_received_at = xthal_get_ccount();
	if (rx_buf[1] == 0) return true;
	// Read the rest of the packet
	if (!uart_read_bytes(UART_NUM_0, rx_buf + 3, rx_buf[1], to)) return false;
	return true;
}

unsigned protocol::ProtocolImpl::get_processing_delay() {
	return (xthal_get_ccount() - last_received_at) / (CONFIG_ESP8266_DEFAULT_CPU_FREQ_MHZ * 1000);
}
