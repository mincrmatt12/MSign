#pragma once

#include <bearssl.h>
#include <stdbool.h>

// Private x509 decoder state
typedef struct wpa_br_x509_insecure_context_ {
	const br_x509_class *vtable;
	bool done_cert;
	br_x509_decoder_context ctx;
} wpa_br_x509_insecure_context;

void wpa_br_x509_insecure_init(wpa_br_x509_insecure_context *ctx);
