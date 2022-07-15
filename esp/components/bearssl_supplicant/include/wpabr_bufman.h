#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wpa_br_tls_bufs {
	uint8_t * rx_buf;
	uint8_t * tx_buf;
	size_t rx_size;
	size_t tx_size;
} wpa_br_tls_bufs_t;

typedef enum wpa_br_tls_src {
	WPA_BR_TLS_SRC_WPA,
	WPA_BR_TLS_SRC_DWHTTP,
	WPA_BR_TLS_SRC_UNUSED
} wpa_br_tls_src_t;

// Fills location with pointers to global buffers for TLS.
//
// Also reserves global lock on it.
//
// Returns 0 on failure
int wpa_br_tls_reserve_buffers(wpa_br_tls_bufs_t *location, wpa_br_tls_src_t src);

// Lets rest of system re-use TLS buffers for HTTPS communications.
void wpa_br_tls_unclaim_buffers(wpa_br_tls_src_t src);

#ifdef __cplusplus
}
#endif
