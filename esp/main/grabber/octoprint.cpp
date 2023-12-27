#include "octoprint.h"
#include "octoprint.cfg.h"
#include "octoprintbitmap.h"

#include <ff.h>
#include <esp_log.h>
#include <memory>
#include "../wifitime.h"
#include "sccfg.h"

static const char * TAG = "octoprint";

namespace octoprint {
	// used to work out if a job file changed without us noticing -- in practice
	// we set this back to -1 whenever we notice the printer not printing anything
	int64_t file_disambig_time = -1;
	uint16_t current_layer_count = 0;
	bool gcode_ok = false;

	void init() {
		// Reset the processed model

		if (f_stat("/cache/printer", NULL) == FR_OK) {
			f_unlink("/cache/printer/layers.bin");
			f_unlink("/cache/printer/bitmaps.bin");
			f_unlink("/cache/printer/boffs.bin");
			f_unlink("/cache/printer/job.gcode");
		}
		else {
			f_mkdir("/cache");
			f_mkdir("/cache/printer");
		}

		file_disambig_time = -1;
	}

	struct OctoprintApiContext {
		const char *headers[2][2] = {
			{"X-Api-Key", api_key.get()},
			{nullptr, nullptr}
		};

		OctoprintApiContext() = default;

		~OctoprintApiContext() {
			close();
		}

		dwhttp::Download request(const char * path) {
			auto result = dwhttp::download_with_callback(host, path, headers);
			result.make_nonclose();
			return result;
		}
		void close() {
			dwhttp::close_connection(host[0] == '_');
		}
	};

	void delete_cached_data() {
		file_disambig_time = -1;
		gcode_ok = false;
		f_unlink("/cache/printer/layers.bin");
		f_unlink("/cache/printer/bitmaps.bin");
		f_unlink("/cache/printer/boffs.bin");
		f_unlink("/cache/printer/job.gcode");

		serial::interface.delete_slot(slots::PRINTER_BITMAP);
	}

	void process_display_name(const char * original_string) {
		int start = 0;
		int end = strlen(original_string);

		// Check if string ends with .gcode
		if (!strcasecmp(".gcode", original_string + end - 6)) {
			end -= 6;
		}

		// Check if string starts with any of the prefixes
		for (auto prefix : filter_prefixes) {
			if (!prefix) break;
			if (!strncmp(original_string, prefix.get(), strlen(prefix.get()))) {
				start = strlen(prefix.get());
				break;
			}
		}

		if (start >= end) {
			start = 0;
			end = strlen(original_string);
		}

		serial::interface.allocate_slot_size(slots::PRINTER_FILENAME, end - start + 1);
		serial::interface.update_slot_at(slots::PRINTER_FILENAME, '\x00', end - start, true, false);
		serial::interface.update_slot_range(slots::PRINTER_FILENAME, original_string + start, 0, end - start);
	}

	// download_path is freed by caller
	bool download_current_gcode(const char * download_path, OctoprintApiContext& ctx) {
		// Assumes file_disambig_time has been updated.
		auto resp = ctx.request(download_path);
		if (!resp.ok()) {
			ESP_LOGE(TAG, "failed to open download for gcode");
			return false;
		}

		GcodeParseProgressTracker pt;

		uint8_t buf[512];
		size_t remain = resp.content_length(), total = remain;
		size_t pos = 0;

		FIL f;
		f_open(&f, "/cache/printer/job.gcode", FA_WRITE | FA_CREATE_ALWAYS);

		while (remain) {
			size_t len = resp(buf, std::min<size_t>(sizeof buf, remain));
			if (!len) {
				ESP_LOGE(TAG, "failed to dw gcode");
				pt.fail();
			}
			UINT bw;
			f_write(&f, buf, len, &bw);
			remain -= len;
			pos += len;

			pt.update(pos * 25 / total);
		}

		f_close(&f);
		ctx.close(); // free memory associated with http before rendering gcode

		// File is downloaded, parse it.
		return process_gcode(current_layer_count);
	}

	bool loop() {
		if (!api_key || !host) {
			sccfg::set_force_disable_screen(slots::ScCfgInfo::PRINTER, true);
			return true;
		}
		OctoprintApiContext ctx;

		slots::PrinterInfo pi;

		auto fail = [&pi](const char *state_message){
			pi.status_code = pi.DISCONNECTED;
			serial::interface.delete_slot(slots::PRINTER_BITMAP_INFO);
			serial::interface.delete_slot(slots::PRINTER_BITMAP);
			serial::interface.update_slot_nosync(slots::PRINTER_INFO, pi);
			serial::interface.update_slot(slots::PRINTER_STATUS, state_message);
			serial::interface.delete_slot(slots::PRINTER_FILENAME);
			serial::interface.sync();
			sccfg::set_force_disable_screen(slots::ScCfgInfo::PRINTER, true);
			return true;
		};

		// Grab the current printer status; if it is an error, reset the slots to offline mode. Otherwise, check
		// the flags and set the mode for next phase.
		{
			auto printer_resp = ctx.request("/api/printer");

			if (!printer_resp.ok()) {
				// Printer is offline.
				return fail(printer_resp.result_code() == 409 ? "Offline" : "Unreachable");
			}

			pi.filament_r = filament_color.color[0];
			pi.filament_g = filament_color.color[1];
			pi.filament_b = filament_color.color[2];

			pi.status_code = pi.READY;

			json::JSONParser jp([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
				if (stack_ptr == 4 && !strcmp(stack[1]->name, "state") && !strcmp(stack[2]->name, "flags")) {
					if ((!strcmp(stack[3]->name, "printing") || !strcmp(stack[3]->name, "paused")) && v.type == v.BOOL && v.bool_val)
						pi.status_code = pi.PRINTING;
				}

				if (stack_ptr == 3 && !strcmp(stack[1]->name, "state") && !strcmp(stack[2]->name, "text") && v.type == v.STR) {
					serial::interface.update_slot(slots::PRINTER_STATUS, v.str_val);
				}
			}, true);

			if (!jp.parse(printer_resp)) {
				return fail("Unreachable");
			}
		}

		sccfg::set_force_disable_screen(slots::ScCfgInfo::PRINTER, false);

		uint32_t filepos_now{};
		bool has_job = false, needs_download = false;
		struct as_deleter {
			void operator()(char * x) const { free(x); }
		};
		std::unique_ptr<char, as_deleter> pending_gcode_download_path;

		// Get the current job info
		{
			auto job_resp = ctx.request("/api/job");

			if (!job_resp.ok())
			{
				ESP_LOGE(TAG, "failed to get /api/job");
				return true;
			}

			json::JSONParser jp([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
				if (stack_ptr == 4 && !strcmp(stack[1]->name, "job") && !strcmp(stack[2]->name, "file")) {
					if (!strcmp(stack[3]->name, "date")) {
						if (v.type == v.NONE) {
							has_job = false;
							needs_download = false;
						}
						else if (v.type == v.INT) {
							has_job = true;
							if (v.int_val == file_disambig_time) {
								needs_download = false;
							}
							else {
								file_disambig_time = v.int_val;
								needs_download = true;
							}
						}
					}
					else if (v.type != v.STR) return;
					else if (!strcmp(stack[3]->name, "display")) {
						process_display_name(v.str_val);
					}
					else if (!strcmp(stack[3]->name, "path")) {
						char *tgt{};
						if (asprintf(&tgt, "/downloads/files/local/%s", v.str_val) < 0) return;
						pending_gcode_download_path.reset(tgt);
					}
				}
				else if (stack_ptr == 3 && !strcmp(stack[1]->name, "progress")) {
					if (!strcmp(stack[2]->name, "completion") && v.is_number()) {
						pi.percent_done = v.as_number() * 100.f;
					}
					else if (!strcmp(stack[2]->name, "filepos") && v.type == v.INT) {
						filepos_now = v.int_val;
					}
					else if (!strcmp(stack[2]->name, "printTimeLeft") && v.is_number()) {
						pi.estimated_print_done = wifi::get_localtime() + 1000 * v.int_val;
					}
				}
			}, true);

			if (!jp.parse(job_resp)) {
				ESP_LOGE(TAG, "failed to parse /api/job");
				return true;
			}
		}

		if (!has_job) {
			delete_cached_data();
			serial::interface.delete_slot(slots::PRINTER_BITMAP_INFO);
			serial::interface.delete_slot(slots::PRINTER_BITMAP);
			serial::interface.delete_slot(slots::PRINTER_FILENAME);
		}
		else {
			if (needs_download) {
				serial::interface.update_slot(slots::PRINTER_INFO, pi);
				if (!download_current_gcode(pending_gcode_download_path.get(), ctx)) {
					gcode_ok = false;
					delete_cached_data();
				}
				else {
					gcode_ok = true;
				}
			}

			pi.total_layer_count = current_layer_count;
			if (gcode_ok) {
				if (pi.status_code == pi.PRINTING)
					send_bitmap_for(filepos_now);
				else
					send_bitmap_for(RANDOM_FILEPOS);
			}
		}

		serial::interface.update_slot(slots::PRINTER_INFO, pi);
		return true;
	}
}
