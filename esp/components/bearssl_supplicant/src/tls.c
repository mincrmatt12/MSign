#include "bearssl_ssl.h"
#include "insecure_x509.h"
#include <bearssl.h>

// EXTERNAL ROUTINES FOR TLS BUFFER MANAGEMENT

#include <wpabr_bufman.h>

// STUFF WE NEED TO GET THE WPA LIB TO WORK EXTERNALLY

#include <stdbool.h>
#include <stdint.h>
#include <esp_libc.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

#ifndef __must_check
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define __must_check __attribute__((__warn_unused_result__))
#else
#define __must_check
#endif /* __GNUC__ */
#endif /* __must_check */

#include <stddef.h>

static inline u16 WPA_GET_BE16(const u8 *a)
{
	return (a[0] << 8) | a[1];
}

static inline void WPA_PUT_BE16(u8 *a, u16 val)
{
	a[0] = val >> 8;
	a[1] = val & 0xff;
}

static inline u16 WPA_GET_LE16(const u8 *a)
{
	return (a[1] << 8) | a[0];
}

static inline void WPA_PUT_LE16(u8 *a, u16 val)
{
	a[1] = val >> 8;
	a[0] = val & 0xff;
}

static inline u32 WPA_GET_BE24(const u8 *a)
{
	return (a[0] << 16) | (a[1] << 8) | a[2];
}

static inline void WPA_PUT_BE24(u8 *a, u32 val)
{
	a[0] = (val >> 16) & 0xff;
	a[1] = (val >> 8) & 0xff;
	a[2] = val & 0xff;
}

static inline u32 WPA_GET_BE32(const u8 *a)
{
	return ((u32) a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

static inline void WPA_PUT_BE32(u8 *a, u32 val)
{
	a[0] = (val >> 24) & 0xff;
	a[1] = (val >> 16) & 0xff;
	a[2] = (val >> 8) & 0xff;
	a[3] = val & 0xff;
}

static inline u32 WPA_GET_LE32(const u8 *a)
{
	return ((u32) a[3] << 24) | (a[2] << 16) | (a[1] << 8) | a[0];
}

static inline void WPA_PUT_LE32(u8 *a, u32 val)
{
	a[3] = (val >> 24) & 0xff;
	a[2] = (val >> 16) & 0xff;
	a[1] = (val >> 8) & 0xff;
	a[0] = val & 0xff;
}

static inline u64 WPA_GET_BE64(const u8 *a)
{
	return (((u64) a[0]) << 56) | (((u64) a[1]) << 48) |
		(((u64) a[2]) << 40) | (((u64) a[3]) << 32) |
		(((u64) a[4]) << 24) | (((u64) a[5]) << 16) |
		(((u64) a[6]) << 8) | ((u64) a[7]);
}

static inline void WPA_PUT_BE64(u8 *a, u64 val)
{
	a[0] = val >> 56;
	a[1] = val >> 48;
	a[2] = val >> 40;
	a[3] = val >> 32;
	a[4] = val >> 24;
	a[5] = val >> 16;
	a[6] = val >> 8;
	a[7] = val & 0xff;
}

static inline u64 WPA_GET_LE64(const u8 *a)
{
	return (((u64) a[7]) << 56) | (((u64) a[6]) << 48) |
		(((u64) a[5]) << 40) | (((u64) a[4]) << 32) |
		(((u64) a[3]) << 24) | (((u64) a[2]) << 16) |
		(((u64) a[1]) << 8) | ((u64) a[0]);
}

static inline void WPA_PUT_LE64(u8 *a, u64 val)
{
	a[7] = val >> 56;
	a[6] = val >> 48;
	a[5] = val >> 40;
	a[4] = val >> 32;
	a[3] = val >> 24;
	a[2] = val >> 16;
	a[1] = val >> 8;
	a[0] = val & 0xff;
}

#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))

#include <wpatls/tls.h>
#define os_memcpy memcpy
#define os_strlen strlen
#include <utils/wpabuf.h>

const static char * TAG = "br_wpa_tls";

#include <esp_log.h>
#include <esp_system.h>

#include <wpabr_bufman.h>

// TLS implementation

struct tls_connection {
	br_ssl_client_context client_ctx;
	wpa_br_x509_insecure_context insecure_ctx;
};

void * tls_init(void) {
	return (void*)1; // never derefed, only checked for NULL
}

void tls_deinit(void *tls_ctx) {
	ESP_LOGV(TAG, "tls deinit");
}

int tls_get_errors(void *tls_ctx) {
	return 0;
}

void tls_connection_deinit(void *tls_ctx, struct tls_connection * conn) {
	if (conn == NULL) return;
	
	ESP_LOGV(TAG, "tls conn deinit");
	
	os_free(conn);
	wpa_br_tls_unclaim_buffers(WPA_BR_TLS_SRC_WPA);
}

struct tls_connection * tls_connection_init(void *tls_ctx) {
	// Allocate a connection
	struct tls_connection * conn = os_zalloc(sizeof(struct tls_connection));
	if (!conn) {
		ESP_LOGE(TAG, "couldn't allocate connection ctx");

		return NULL;
	}
	wpa_br_tls_bufs_t buffers;
	// Try to reserve the TLS buffers
	if (!wpa_br_tls_reserve_buffers(&buffers, WPA_BR_TLS_SRC_WPA)) {
		ESP_LOGW(TAG, "couldn't reserve buffer");

		os_free(conn);
		return NULL;
	}
	// Initialize bearssl context:
	// First, setup the x509 lack-of-validator
	wpa_br_x509_insecure_init(&conn->insecure_ctx);
	// Then, setup the tls connection itself
	static const uint16_t suites[] = {
		BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
		BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_CCM,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_CCM,
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8,
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
		BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
		BR_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		BR_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
		BR_TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256,
		BR_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256,
		BR_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384,
		BR_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384,
		BR_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		BR_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
		BR_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
		BR_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
		BR_TLS_RSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_RSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_RSA_WITH_AES_128_CCM,
		BR_TLS_RSA_WITH_AES_256_CCM,
		BR_TLS_RSA_WITH_AES_128_CCM_8,
		BR_TLS_RSA_WITH_AES_256_CCM_8,
		BR_TLS_RSA_WITH_AES_128_CBC_SHA256,
		BR_TLS_RSA_WITH_AES_256_CBC_SHA256,
		BR_TLS_RSA_WITH_AES_128_CBC_SHA,
		BR_TLS_RSA_WITH_AES_256_CBC_SHA,
		BR_TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
		BR_TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
		BR_TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
		BR_TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
		BR_TLS_RSA_WITH_3DES_EDE_CBC_SHA
	};

	/*
	 * All hash functions are activated.
	 * Note: the X.509 validation engine will nonetheless refuse to
	 * validate signatures that use MD5 as hash function.
	 */
	static const br_hash_class *hashes[] = {
		&br_md5_vtable,
		&br_sha1_vtable,
		&br_sha224_vtable,
		&br_sha256_vtable,
		&br_sha384_vtable,
		&br_sha512_vtable
	};

	/*
	 * Reset client context and set supported versions from TLS-1.0
	 * to TLS-1.2 (inclusive).
	 */
	br_ssl_client_zero(&conn->client_ctx);
	br_ssl_engine_set_versions(&conn->client_ctx.eng, BR_TLS10, BR_TLS12);

	br_ssl_engine_set_suites(&conn->client_ctx.eng, suites, (sizeof suites) / (sizeof suites[0]));
	br_ssl_client_set_default_rsapub(&conn->client_ctx);
	br_ssl_engine_set_default_rsavrfy(&conn->client_ctx.eng);
	br_ssl_engine_set_default_ecdsa(&conn->client_ctx.eng);
	/*
	 * Set supported hash functions, for the SSL engine and for the
	 * X.509 engine.
	 */
	for (int id = br_md5_ID; id <= br_sha512_ID; id ++) {
		const br_hash_class *hc;

		hc = hashes[id - 1];
		br_ssl_engine_set_hash(&conn->client_ctx.eng, id, hc);
	}

	/*
	 * Link the X.509 engine in the SSL engine.
	 */
	br_ssl_engine_set_x509(&conn->client_ctx.eng, &conn->insecure_ctx.vtable);

	/*
	 * Set the PRF implementations.
	 */
	br_ssl_engine_set_prf10(&conn->client_ctx.eng, &br_tls10_prf);
	br_ssl_engine_set_prf_sha256(&conn->client_ctx.eng, &br_tls12_sha256_prf);
	br_ssl_engine_set_prf_sha384(&conn->client_ctx.eng, &br_tls12_sha384_prf);

	/*
	 * Symmetric encryption. We use the "default" implementations
	 * (fastest among constant-time implementations).
	 */
	br_ssl_engine_set_default_aes_cbc(&conn->client_ctx.eng);
	br_ssl_engine_set_default_aes_ccm(&conn->client_ctx.eng);
	br_ssl_engine_set_default_aes_gcm(&conn->client_ctx.eng);
	br_ssl_engine_set_default_des_cbc(&conn->client_ctx.eng);
	br_ssl_engine_set_default_chapol(&conn->client_ctx.eng);

	// Set IO buffers
	br_ssl_engine_set_buffers_bidi(&conn->client_ctx.eng, buffers.rx_buf, buffers.rx_size, buffers.tx_buf, buffers.tx_size);

	// Shutdown really just acts as a reset.
	tls_connection_shutdown(NULL, conn);

	// Connection is now ready
	return conn;
}

int tls_connection_get_random(void * tls_ctx, struct tls_connection * conn, struct tls_random *rand_out) {
	memset(rand_out, 0, sizeof(*rand_out));
	// HACKERY: not in bearssl, so we do it ourselves
	if (conn->client_ctx.eng.application_data != 1) {
		// Handshake not over
		return -1;
	}
	// Copy pointers around
	rand_out->client_random = conn->client_ctx.eng.client_random;
	rand_out->server_random = conn->client_ctx.eng.server_random;
	rand_out->client_random_len = 32;
	rand_out->server_random_len = 32;
	return 0;
}

int tls_connection_export_key(void *tls_ctx, struct tls_connection *conn, const char *label, u8 *out, size_t out_len) {
	return br_ssl_key_export(&conn->client_ctx.eng, out, out_len, label, NULL, 0) ? 0 : -1;
}

int tls_connection_established(void *tls_ctx, struct tls_connection *conn) {
	return conn->client_ctx.eng.application_data >= 1;
}

int tls_connection_shutdown(void *tls_ctx, struct tls_connection *conn) {
	ESP_LOGV(TAG, "tls shutdown");
	// Feed some entropy into bearssl
	uint32_t block[8];
	for (int i = 0; i < 8; ++i) block[i] = esp_random();
	br_ssl_engine_inject_entropy(&conn->client_ctx.eng, &block, sizeof block);

	// Finally, we prepare the connection by resetting it
	// We never support sessions (because reasons)
	br_ssl_client_reset(&conn->client_ctx, NULL, false);
	return 0;
}

int tls_connection_set_params(void *tls_ctx, struct tls_connection *conn, const struct tls_connection_params *params) {
	// Verify no client certificate is in use (we don't support them)
	
	if (params->client_cert_blob && params->private_key_blob) {
		ESP_LOGE(TAG, "currently no support is implemented for client certs");
		return -1;
	}
	
	return 0;
}

// We use our own cipher suite
int tls_connection_set_cipher_list(void *tls_ctx, struct tls_connection *conn, u8 *cph) {return 0;}

// Never called
int tls_get_cipher(void * tls_ctx, struct tls_connection *conn, char *buf, size_t buflen) {
	return -1;
}

// Never used
int tls_connection_enable_workaround(void *tls_ctx,
				     struct tls_connection *conn)
{
	return -1;
}

int tls_connection_get_failed(void *tls_ctx, struct tls_connection *conn)
{
	return 0;
}

int tls_connection_get_read_alerts(void *tls_ctx, struct tls_connection *conn)
{
	return 0;
}

int tls_connection_get_write_alerts(void *tls_ctx, struct tls_connection *conn)
{
	return 0;
}

int tls_global_set_params(void *tls_ctx,
			  const struct tls_connection_params *params)
{
	return -1;
}

int tls_connection_set_session_ticket_cb(void *tls_ctx,
					 struct tls_connection *conn,
					 tls_session_ticket_cb cb,
					 void *ctx)
{
	return -1;
}

int tls_global_set_verify(void *tls_ctx, int check_crl)
{
	return -1;
}

int tls_connection_set_verify(void *tls_ctx, struct tls_connection *conn,
			      int verify_peer)
{
	return -1;
}

int tls_connection_resumed(void *tls_ctx, struct tls_connection *conn) {
	return 0; // session resumption is never enabled
}

static int wpabuf_append(struct wpabuf ** buf, void * data, size_t length) {
	int offset = 0;
	if (*buf == NULL) {
		*buf = wpabuf_alloc(length);
		if (*buf == NULL) {
			ESP_LOGE(TAG, "No memory for buf");
			return -1;
		}
	}
	else {
		if (wpabuf_resize(buf, length))
		{
			ESP_LOGE(TAG, "No memory to append to.");
			return -1;
		}
	}
	wpabuf_put_data(*buf, data, length);
	return 0;
}

struct wpabuf * tls_connection_handshake(void *tls_ctx, struct tls_connection *conn, const struct wpabuf *in_data, struct wpabuf **appl_data) {
	// Inject in data first, then extract out data
	struct wpabuf * output_records = NULL;
	unsigned int state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	void * buftarget;
	size_t length, soffset = 0;

	ESP_LOGV(TAG, "tls_connection_handshake start w/ state=%d, dat = %p", state, in_data);

	if (appl_data) *appl_data = NULL;

	// Engine expects records and we have some, send them in
	while (state & BR_SSL_RECVREC && in_data && soffset < wpabuf_len(in_data)) {
		buftarget = br_ssl_engine_recvrec_buf(&conn->client_ctx.eng, &length);

		if (length < wpabuf_len(in_data) - soffset && length) {
			ESP_LOGV(TAG, "Incoming data too large, truncating %d --> %d", (int)(wpabuf_len(in_data) - soffset), (int)length);
		}

		ESP_LOGV(TAG, "hs: injecting incoming records");

		if (length > wpabuf_len(in_data) - soffset) length = wpabuf_len(in_data) - soffset;

		// Inject records
		memcpy(buftarget, wpabuf_head(in_data) + soffset, length);
		br_ssl_engine_recvrec_ack(&conn->client_ctx.eng, length);
		state = br_ssl_engine_current_state(&conn->client_ctx.eng);
		soffset += length;
	}

	if (in_data && soffset < wpabuf_len(in_data)) {
		ESP_LOGE(TAG, "incoming data present but engine not ready for it");

		return NULL;
	}

	state = br_ssl_engine_current_state(&conn->client_ctx.eng);

	// Now, are there any records to be sent out?
	while (state & BR_SSL_SENDREC) {
		ESP_LOGV(TAG, "hs: getting outgoing records");
		// Get it and place it into a wpabuf
		buftarget = br_ssl_engine_sendrec_buf(&conn->client_ctx.eng, &length);
		ssize_t offset = wpabuf_append(&output_records, buftarget, length);
		if (offset < 0) {
			wpabuf_free(output_records);
			ESP_LOGE(TAG, "Failed to allocate more space for outgoing buf");
			return NULL;
		}
		ESP_LOGV(TAG, "created outoing buf: %p; len %d", output_records, (int)length);
		// Ack
		br_ssl_engine_sendrec_ack(&conn->client_ctx.eng, length);
		state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	}

	// Output data is prepared; is there any application data?
	if (state & BR_SSL_RECVAPP && appl_data) {
		buftarget = br_ssl_engine_recvapp_buf(&conn->client_ctx.eng, &length);

		*appl_data = wpabuf_alloc_copy(buftarget, length);
		if (*appl_data) { // If allocation didn't fail
			br_ssl_engine_recvapp_ack(&conn->client_ctx.eng, length);
		}
		else {
			ESP_LOGW(TAG, "failed to allocate incoming application data");
		}
	}

	// If no output data, make some empty buffer to avoid triggering errors
	if (!output_records) {
		output_records = wpabuf_alloc(0);
	}

	// Return received data
	return output_records;
}

struct wpabuf * tls_connection_encrypt(void *tls_ctx, struct tls_connection *conn, const struct wpabuf *in_data) {
	struct wpabuf * output_records = NULL;
	unsigned int state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	void * buftarget;
	size_t length;

	size_t pos = 0, remain = wpabuf_len(in_data);
	bool flushed = false;
	ESP_LOGV(TAG, "tls_encrypt: %p; dat: %p, st: %d, l: %d", conn, in_data, state, (int)remain);
	while (pos < wpabuf_len(in_data)) {
		state = br_ssl_engine_current_state(&conn->client_ctx.eng);
		ESP_LOGV(TAG, "tls_encrypt loop: %d, state %d", (int)pos, state);
		if (state == BR_SSL_CLOSED) {
			ESP_LOGW(TAG, "encrypt conn close");
			return output_records; // connection faield
		}
		if (state & BR_SSL_SENDAPP) {
			buftarget = br_ssl_engine_sendapp_buf(&conn->client_ctx.eng, &length);
			ESP_LOGV(TAG, "tls_encrypt: sendapp %d %d %d", (int)length, (int)remain, state);
			if (length >= remain) length = remain;
			memcpy(buftarget, wpabuf_head(in_data) + pos, length);
			br_ssl_engine_sendapp_ack(&conn->client_ctx.eng, length);
			pos += length;
			remain -= length;
		}
		else if (state & BR_SSL_SENDREC) {
			buftarget = br_ssl_engine_sendrec_buf(&conn->client_ctx.eng, &length);
			ssize_t offset = wpabuf_append(&output_records, buftarget, length);
			if (offset < 0) {
				wpabuf_free(output_records);
				ESP_LOGE(TAG, "Failed to allocate more space for outgoing buf");
				return NULL;
			}
			br_ssl_engine_sendrec_ack(&conn->client_ctx.eng, length);
		}
		else if (!flushed) {
			flushed = true;
			br_ssl_engine_flush(&conn->client_ctx.eng, false);
			continue;
		}
		else {
			ESP_LOGE(TAG, "Unable to encrypt (bad state)");
			wpabuf_free(output_records);
			return NULL;
		}
		flushed = false;
	}
	ESP_LOGV(TAG, "loop done");
	br_ssl_engine_flush(&conn->client_ctx.eng, output_records == NULL);
	state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	// Now flush out the content
	while (state & BR_SSL_SENDREC) {
		buftarget = br_ssl_engine_sendrec_buf(&conn->client_ctx.eng, &length);
		if (wpabuf_append(&output_records, buftarget, length) < 0) {
			wpabuf_free(output_records);
			ESP_LOGE(TAG, "Failed to allocate more space for outgoing buf");
			return NULL;
		}
		br_ssl_engine_sendrec_ack(&conn->client_ctx.eng, length);
		state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	}
	return output_records;
}

struct wpabuf * tls_connection_decrypt(void *tls_ctx, struct tls_connection *conn, const struct wpabuf *in_data) {
	struct wpabuf * output_records = NULL;
	unsigned int state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	void * buftarget;
	size_t length, soffset = 0;

	state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	if (state == BR_SSL_CLOSED) {
		ESP_LOGW(TAG, "decrypt conn close");
		return NULL; // connection faield
	}

	if (!(state & BR_SSL_RECVREC)) {
		ESP_LOGE(TAG, "not ready for incoming records");
		return NULL;
	}

	// Send incoming data
	while (soffset < wpabuf_len(in_data)) {
		state = br_ssl_engine_current_state(&conn->client_ctx.eng);
		if (!(state & BR_SSL_RECVREC)) {
			ESP_LOGE(TAG, "not ready for incoming records");
			return NULL;
		}

		buftarget = br_ssl_engine_recvrec_buf(&conn->client_ctx.eng, &length);

		if (length > wpabuf_len(in_data) - soffset) length = wpabuf_len(in_data) - soffset;

		memcpy(buftarget, wpabuf_head(in_data) + soffset, length);
		br_ssl_engine_recvrec_ack(&conn->client_ctx.eng, length);

		soffset += length;
	}

	ESP_LOGV(TAG, "tls_decrypt: %p; dat: %p, st: %d", conn, in_data, state);

	state = br_ssl_engine_current_state(&conn->client_ctx.eng);

	// Get decrypted data
	if (!(state & BR_SSL_RECVAPP)) {
		ESP_LOGE(TAG, "no app data ready");
		return NULL;
	}

	while (state & BR_SSL_RECVAPP) {
		buftarget = br_ssl_engine_recvapp_buf(&conn->client_ctx.eng, &length);
		if (wpabuf_append(&output_records, buftarget, length)) {
			wpabuf_free(output_records);
			ESP_LOGE(TAG, "failed to allocate result buf");
			return NULL;
		}
		br_ssl_engine_recvapp_ack(&conn->client_ctx.eng, length);
		state = br_ssl_engine_current_state(&conn->client_ctx.eng);
	}

	return output_records;
}
