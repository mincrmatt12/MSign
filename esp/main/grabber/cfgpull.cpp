#include "cfgpull.cfg.h"
#include "cfgpull.h"

#include "../dwhttp.h"
#include "esp_system.h"
#include <esp_log.h>

#ifndef SIM
#include <esp_ota_ops.h>
#include <sdkconfig.h>
#endif

#include <memory>

#include <bearssl_hmac.h>
#include <bearssl_hash.h>

#include "../sd.h"
#include "../common/util.h"
#include "../serial.h"

static const char * const TAG = "cfgpull";

namespace cfgpull {
	bool pull_single_file_for_update(dwhttp::Download &&resp, const char *target_file, uint16_t &crc_out) {
		if (!resp.ok()) {
			return false;
		}

		crc_out = 0;

		FIL f;
		f_open(&f, target_file, FA_WRITE | FA_CREATE_ALWAYS);

		ESP_LOGD(TAG, "downloading %s", target_file);

		size_t remain = resp.content_length();
		uint8_t buf[512];
		while (remain) {
			size_t len = resp(buf, std::min<size_t>(512, remain));
			if (!len) {
				ESP_LOGW(TAG, "failed to dw update");
				f_close(&f);
				return false;
			}
			UINT bw;
			crc_out = util::compute_crc(buf, len, crc_out);
			if (f_write(&f, buf, len, &bw)) {
				ESP_LOGW(TAG, "failed to write update");
				f_close(&f);
				return false;
			}
			remain -= len;
		}
		f_close(&f);
		resp.make_nonclose();
		return true;
	}
}

void cfgpull::init() {
	if (!enabled) ESP_LOGI(TAG, "cfgpull is not turned on");
}

extern "C" void * gEapSm;

bool cfgpull::loop() {
	if (!pull_host && !enabled) return true;
	// Check if endpoint url is configured
	if (!pull_host) {
		ESP_LOGE(TAG, "cfgpull is enabled but url is missing.");
		return false;
	}

	bool new_config = false, new_system = false;
	{
		auto resp = dwhttp::open_site(pull_host, "/a/versions");

#ifndef SIM
		{
			char metrics_buf[32];
			auto metrics_sender = resp.begin_header("X-MSign-Metrics");
			resp.write("mem_free ");
			snprintf(metrics_buf, sizeof metrics_buf, "%d", esp_get_free_heap_size());
			resp.write(metrics_buf);
			resp.write("; mem_free_dram ");
			snprintf(metrics_buf, sizeof metrics_buf, "%d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
			resp.write(metrics_buf);
			resp.write("; esp_tickcount ");
			snprintf(metrics_buf, sizeof metrics_buf, "%d", xTaskGetTickCount());
			resp.write(metrics_buf);
			resp.write("; grab_task_watermark ");
			snprintf(metrics_buf, sizeof metrics_buf, "%d", (int)uxTaskGetStackHighWaterMark(NULL));
			resp.write(metrics_buf);
			resp.write("; geapsm_allocated ");
			snprintf(metrics_buf, sizeof metrics_buf, "%d", (int)(gEapSm != NULL));
			resp.write(metrics_buf);
		}
#endif

		resp.end_body();
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
			else if (strcmp(stack[1]->name, "config") == 0 && !only_firm) {
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

	// Force return if disabled -- do now to still send metrics updates.
#ifdef CONFIG_DISABLE_CFGPULL
	return true;
#endif
	if (!enabled) return true;


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
	const char * headers[][2] = {
		{"Authorization", (char *)authbuf.get()},
		{nullptr, nullptr}
	};
	
	if (new_config) {
		auto resp = dwhttp::download_with_callback(pull_host_copy, "/a/conf.json", headers);

		if (!resp.ok()) {
			ESP_LOGW(TAG, "failed to dw conf");
			return false;
		}

		resp.make_nonclose();
		// Pull config.
		FIL f;
		f_open(&f, "0:/config.json.tmp", FA_CREATE_ALWAYS | FA_WRITE);

		size_t remain = resp.content_length();
		uint8_t buf[64];
		while (remain) {
			size_t len = resp(buf, std::min<size_t>(64, remain));
			if (!len) {
				ESP_LOGW(TAG, "failed to dw cfg");
				f_close(&f);
				return false;
			}
			UINT bw;
			if (f_write(&f, buf, len, &bw)) {
				ESP_LOGW(TAG, "failed to write cfg");
			}
			remain -= len;
		}
		f_close(&f);
		f_unlink("0:/config.json.old");
		f_rename("0:/config.json", "0:/config.json.old");
		f_rename("0:/config.json.tmp", "0:/config.json");
		if (!config::parse_config_from_sd()) {
			ESP_LOGW(TAG, "new config failed to parse, reinstating old config");
			f_unlink("0:/config.json.bad");
			f_rename("0:/config.json", "0:/config.json.bad");
			f_rename("0:/config.json.old", "0:/config.json");
			config::parse_config_from_sd();
			enabled = false; //stop parsing
			return false;
		}
		// new config installed ok
		ESP_LOGI(TAG, "new config pulled succesfully");
		grabber::reload_all();
		grabber::refresh(slots::protocol::GrabberID::ALL);
	}

	if (new_system) {
		{
			slots::WebuiStatus current_status;
			current_status.flags = slots::WebuiStatus::RECEIVING_SYSUPDATE;
			serial::interface.update_slot(slots::WEBUI_STATUS, current_status);
		}
		// download files
		uint16_t esp_csum, stm_csum;

		f_mkdir("0:/upd");

		if (!pull_single_file_for_update(
			dwhttp::download_with_callback(pull_host_copy, "/a/esp.bin", headers),
			"/upd/esp.bin",
			esp_csum
		) || !pull_single_file_for_update(
			dwhttp::download_with_callback(pull_host_copy, "/a/stm.bin", headers),
			"/upd/stm.bin",
			stm_csum
		)) {
			ESP_LOGE(TAG, "failed to download sysupgrade files, bail");
			{
				slots::WebuiStatus current_status;
				current_status.flags = slots::WebuiStatus::LAST_RX_FAILED;
				serial::interface.update_slot(slots::WEBUI_STATUS, current_status);
			}
			return false;
		}

		// write checksums
		FIL f;
		f_open(&f, "/upd/chck.sum", FA_WRITE | FA_CREATE_ALWAYS);
		UINT bw;
		f_write(&f, &esp_csum, 2, &bw);
		f_write(&f, &stm_csum, 2, &bw);
		f_close(&f);

		// Set the update state for update
		f_open(&f, "/upd/state", FA_WRITE | FA_CREATE_ALWAYS);
		f_putc(0, &f);
		f_close(&f);

		ESP_LOGI(TAG, "finished pulling update, rebooting");
	}

	dwhttp::close_connection(pull_host_copy[0] == '_');
	
	if (new_system) serial::interface.reset();

	return true;
}
