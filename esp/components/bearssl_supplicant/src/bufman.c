#ifndef SIM
#include "esp_attr.h"
#include <sdkconfig.h>
#else
#define IRAM_ATTR
#define CONFIG_WPA_BR_TLS_OUT_SIZE 2048
#endif
#include <wpabr_bufman.h>
#include <bearssl.h>
#include <FreeRTOS.h>

volatile wpa_br_tls_src_t current_tls_reservation = WPA_BR_TLS_SRC_UNUSED;

// These are kept as staticly allocated to avoid heap fragmentation.
// The rest of the context is freed as necessary (as it technically isn't required while the HTTP one is running)
//
// Placed in IRAM to avoid putting unnecessary pressure on DRAM heap to allow more tasks to be allocated

IRAM_ATTR uint8_t ssl_io_buffer_in [BR_SSL_BUFSIZE_INPUT];
uint8_t ssl_io_buffer_out[CONFIG_WPA_BR_TLS_OUT_SIZE + 85]; // 85 bytes of output overhead

int wpa_br_tls_reserve_buffers(wpa_br_tls_bufs_t *location, wpa_br_tls_src_t src) {
	portENTER_CRITICAL();
	if (current_tls_reservation != WPA_BR_TLS_SRC_UNUSED) {
		portEXIT_CRITICAL();
		// In use
		return 0;
	}
	current_tls_reservation = src;
	portEXIT_CRITICAL();
	// Store result into location
	location->tx_size = sizeof(ssl_io_buffer_out);
	location->rx_size = sizeof(ssl_io_buffer_in);

	location->tx_buf = ssl_io_buffer_out;
	location->rx_buf = ssl_io_buffer_in;
	return 1;
}

void wpa_br_tls_unclaim_buffers(wpa_br_tls_src_t src) {
	if (current_tls_reservation != src) {
		return;
	}
	current_tls_reservation = WPA_BR_TLS_SRC_UNUSED;
}
