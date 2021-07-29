#include "cfgpull.cfg.h"
#include "cfgpull.h"

#include "../dwhttp.h"
#include <esp_log.h>

#ifndef SIM
#include <esp_ota_ops.h>
#include <sdkconfig.h>
#endif

#include <memory>

#include <bearssl_hmac.h>
#include <bearssl_hash.h>

static const char * TAG = "cfgpull";

void cfgpull::init() {
	if (!enabled) ESP_LOGI(TAG, "cfgpull is not turned on");
}

bool cfgpull::loop() {
#ifdef CONFIG_DISABLE_CFGPULL
	return true;
#endif
	if (!enabled) return true;

	// Check if endpoint url is configured
	if (!pull_host) {
		ESP_LOGE(TAG, "cfgpull is enabled but url is missing.");
		return false;
	}

	bool new_config = false, new_system = false;
	{
		auto resp = dwhttp::download_with_callback(pull_host, "/a/versions");
		if (!resp.ok()) return false;

		json::JSONParser jp([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& jv){
			if (stack_ptr != 2) return;

			if (strcmp(stack[1]->name, "firm") == 0 && jv.type == jv.STR) {
#ifndef SIM
				new_system = strcmp(
					jv.str_val,
					esp_ota_get_app_description()->version
				) != 0;
				if (new_system) ESP_LOGI(TAG, "detected new system version");
#endif
			}
			else if (strcmp(stack[1]->name, "config") == 0) {
				if (jv.int_val != version_id) {
					new_config = true;
					ESP_LOGI(TAG, "detected new config version");
				}
			}
		});

		if (!jp.parse(resp)) {
			ESP_LOGW(TAG, "couldn't parse version json");
			return false;
		}

		// If new updates are available, we should try to keep-alive
		if (new_config || new_system) resp.make_nonclose();
		else {
			// No new updates, return now
			return true;
		}
	}

	// New updates found, authenticate with server.
	std::unique_ptr<uint8_t []> authbuf;
	{
		auto resp = dwhttp::download_with_callback(pull_host, "/a/challenge");
		if (!resp.ok()) return false;

		if (resp.is_unknown_length()) return false;
		// allocate resp buf with size for 5 "Chip", 1 separator, 64 hash nybbles and 1 terminator (71)
		authbuf.reset(new uint8_t[resp.content_length() + 71]);
		bzero(authbuf.get(), resp.content_length() + 71);

		strcpy((char *)authbuf.get(), "Chip ");

		size_t remain = resp.content_length();
		uint8_t * place = authbuf.get() + 5;
		while (remain) {
			size_t amt = resp(place, remain);
			if (!amt) return false;
			place += amt;
			remain -= amt;
		}

		ESP_LOGD(TAG, "got authstr %s", authbuf.get());
		resp.make_nonclose();

		// Perform hmac
		authbuf[resp.content_length() + 5] = ':';

		br_hmac_key_context kctx;
		br_hmac_key_init(&kctx, &br_sha256_vtable, pull_secret, strlen(pull_secret));

		br_hmac_context hctx;
		br_hmac_init(&hctx, &kctx, 32);
		br_hmac_update(&hctx, authbuf.get() + 5, resp.content_length());

		// Output to buffer and convert backwards.
		br_hmac_out(&hctx, authbuf.get() + resp.content_length() + 1 + 5);

		// Convert to hex
		for (ssize_t pos = 31; pos >= 0; --pos) {
			const static char * hexdigits = "0123456789abcdef";
			authbuf[resp.content_length() + 6 + pos * 2 + 1] = hexdigits[authbuf[5 + resp.content_length() + 1 + pos] % 0x10];
			authbuf[resp.content_length() + 6 + pos * 2] = hexdigits[authbuf[5 + resp.content_length() + 1 + pos] >> 4];
		}

		ESP_LOGD(TAG, "signed authstr %s", authbuf.get());
	}

	// Perform config update first.
	const char * pull_host_copy = pull_host; // if we overwrite config during this don't blow up.
	
	if (new_config) {
		const char * headers[][2] = {
			{"Authorization", (char *)authbuf.get()},
			{nullptr, nullptr}
		};
		auto resp = dwhttp::download_with_callback(pull_host_copy, "/a/conf.json", headers);

		if (!resp.ok()) {
			ESP_LOGW(TAG, "failed to dw conf");
			return false;
		}

		resp.make_nonclose();
		// Pull config.

	}

	dwhttp::close_connection(pull_host_copy[0] == '_');
	return true;
}
