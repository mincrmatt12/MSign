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

	BaseType_t res;
	if ((res = xTaskCreate([](void *ptr){
		((protocol::ProtocolImpl *)ptr)->rx_task();
	}, "rxST", configMINIMAL_STACK_SIZE, this, 10, &rx_thread)) != pdPASS) {
		ESP_LOGE(TAG, "failed to create rxST %d", res);
		ESP_LOGE(TAG, "had %d bytes", esp_get_free_heap_size());
	}
}

void protocol::ProtocolImpl::send_pkt(const void *pkt) {
	uart_write_bytes(UART_NUM_0, (const char *)pkt, reinterpret_cast<const uint8_t *>(pkt)[1] + 3);
}

void protocol::ProtocolImpl::continue_rx() {
	xTaskNotify(rx_thread, 0x5555, eSetValueWithOverwrite);
}

void protocol::ProtocolImpl::rx_task() {
	while (true) {
		// Wait for a sync byte
		uart_read_bytes(UART_NUM_0, rx_buf, 3, portMAX_DELAY);
		while (rx_buf[0] != 0xa5) {
			rx_buf[0] = rx_buf[1];
			rx_buf[1] = rx_buf[2];
			uart_read_bytes(UART_NUM_0, rx_buf + 2, 1, portMAX_DELAY);
		}
		// Set last received at
		last_received_at = xthal_get_ccount();
		// Read the rest of the packet
		uart_read_bytes(UART_NUM_0, rx_buf + 3, rx_buf[1], portMAX_DELAY);
		// Signal packet completed
		on_pkt();
		// Wait for sync
		uint32_t f = 0;
		while (f != 0x5555) {
			xTaskNotifyWait(0, 0xffff'ffff, &f, portMAX_DELAY);
		}
	}
}

unsigned protocol::ProtocolImpl::get_processing_delay() {
	return (xthal_get_ccount() - last_received_at) / (CONFIG_ESP8266_DEFAULT_CPU_FREQ_MHZ * 1000);
}
