#include "insecure_x509.h"

// Based somewhat on the arduino code, but even crappier: for EAP verification we don't really have
// a good way of doing the cert verification anyways and just accept whatever.

// Callback on the first byte of any certificate
static void insecure_start_chain(const br_x509_class **ctx, const char *server_name) {
	wpa_br_x509_insecure_context *xc = (wpa_br_x509_insecure_context *)ctx;
	br_x509_decoder_init(&xc->ctx, NULL, NULL);
	xc->done_cert = false;
	(void)server_name;
}

// Callback for each certificate present in the chain (but only operates
// on the first one by design).
static void insecure_start_cert(const br_x509_class **ctx, uint32_t length) {
	(void) ctx;
	(void) length;
}

// Callback for each byte stream in the chain.  Only process first cert.
static void insecure_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
	wpa_br_x509_insecure_context *xc = (wpa_br_x509_insecure_context *)ctx;
	// Don't process anything but the first certificate in the chain
	if (!xc->done_cert) {
		br_x509_decoder_push(&xc->ctx, (const void*)buf, len);
	}
}

// Callback on individual cert end.
static void insecure_end_cert(const br_x509_class **ctx) {
	wpa_br_x509_insecure_context *xc = (wpa_br_x509_insecure_context *)ctx;
	xc->done_cert = true;
}

// Callback when complete chain has been parsed.
// Return 0 on validation success, !0 on validation error
static unsigned insecure_end_chain(const br_x509_class **ctx) {
	const wpa_br_x509_insecure_context *xc = (const wpa_br_x509_insecure_context *)ctx;
	if (!xc->done_cert) {
		return 1; // error
	}

	return 0;
}

// Return the public key from the validator (set by x509_minimal)
static const br_x509_pkey *insecure_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
	const wpa_br_x509_insecure_context *xc = (const wpa_br_x509_insecure_context *)ctx;
	if (usages != NULL) {
		*usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
	}
	return &xc->ctx.pkey;
}

//  Set up the x509 insecure data structures for BearSSL core to use.
void wpa_br_x509_insecure_init(wpa_br_x509_insecure_context *ctx) {
	static const br_x509_class br_x509_insecure_vtable = {
		sizeof(wpa_br_x509_insecure_context),
		insecure_start_chain,
		insecure_start_cert,
		insecure_append,
		insecure_end_cert,
		insecure_end_chain,
		insecure_get_pkey
	};

	memset(ctx, 0, sizeof * ctx);
	ctx->vtable = &br_x509_insecure_vtable;
	ctx->done_cert = false;
}
