#include "upd.h"
#include <cstring>
#include <esp_log.h>
#include <ff.h>
#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>

const static char * const TAG="w_updater";

namespace upd {
	bool copy_updated_file(const char* tfname, FIL* source, int length) {
		FIL target; f_open(&target, tfname, FA_WRITE | FA_CREATE_ALWAYS);
		int bytes_since_last_yield = 0;
		uint8_t buf[256];
		UINT br;
		for (int i = 0; i < length; i += 256) {
			int size_of_tx = (length - i >= 256) ? 256 : (length - i);
			if (f_eof(source)) {
				f_close(&target);
				ESP_LOGE(TAG, "failed to copy");
				return false;
			}
			f_read(source, buf, size_of_tx, &br);
			UINT bw;
			f_write(&target, buf, br, &bw);
			bytes_since_last_yield += bw;
			if (bytes_since_last_yield > 4096) {
				vTaskDelay(10);
				bytes_since_last_yield = 0;
			}
		}

		f_close(&target);
		return true;
	}

	static const struct update_archive_entry {
		const char *target, *arcname; int arclen;

		template<size_t len>
		constexpr update_archive_entry(const char * target, const char (&arcname)[len]) :
			target(target),
			arcname(arcname),
			arclen(len-1)
		{}
	} webui_entries[] = {
#define e(v) {"/web/" v, v "/"}, {"/web/" v ".gz", v ".gz/"} 
		e("page.html"),
		e("page.js"),
		e("page.css")
#undef e
	};

	void update_website() {
		// check for the webui.ar file
		if (f_stat("/upd/webui.ar", NULL) != FR_OK) {
			ESP_LOGE(TAG, "tried to update web but no webui.ar file");
			return;
		}

		// otherwise, prepare by BLOWING THE OLD SHIT TO LITTE BITS

		FIL arF; f_open(&arF, "/upd/webui.ar", FA_READ);
	 	uint8_t magic[8];
		UINT br;
		int files_read = 0;
		if ((f_read(&arF, magic, sizeof(magic), &br), br) != sizeof(magic) ||
				memcmp(magic, "!<arch>\n", sizeof(magic)) ) {
			f_close(&arF);
	invalidformat:
			ESP_LOGE(TAG, "what kind of shit are you trying to pull here? give me a .ar file...");
			goto failure;
		}
		
		ESP_LOGD(TAG, "removing old files...");

		for (const auto& entry : webui_entries)
			f_unlink(entry.target);
		f_unlink("/web/etag"); // ignore failures here

		while (files_read < sizeof(webui_entries)/sizeof(update_archive_entry)) {
			uint8_t fileHeader[60];
			do {
				if (f_read(&arF, &fileHeader[0], 1, &br) != FR_OK || !br) {
					ESP_LOGE(TAG, "invalid ar file");
					f_close(&arF);
					goto failure;
				}
			} while (fileHeader[0] == '\n');
			f_read(&arF, &fileHeader[1], sizeof(fileHeader)-1, &br);
			if (br != sizeof(fileHeader)-1) {
				f_close(&arF);
				goto failure;
			}
			fileHeader[58] = 0;

			const update_archive_entry * entry{};
			for (const auto& possible : webui_entries) {
				if (memcmp(fileHeader, (const void*)possible.arcname, possible.arclen) == 0) {
					entry = &possible;
					break;
				}
			}
			// check the filename
			if (!entry) {
				ESP_LOGD(TAG, "unknown entry");
				f_close(&arF);
				
				goto failure;
			}

			// ok, now get the size
			int length;
			if (1 != sscanf((char *)(fileHeader + 48), "%d", &length) || !length) {
				goto invalidformat;
			}

			ESP_LOGI(TAG, "updating: %s", entry->target);
			if (copy_updated_file(entry->target, &arF, length))
				++files_read;
			else goto failure;
		}
		f_close(&arF);

		ESP_LOGI(TAG, "finished ui update.");
failure: // clean up
		f_unlink("/upd/state");
		f_unlink("/upd/webui.ar");
		f_rmdir("/upd");
	}


	void update_cacerts() {
		// Expects certs.bin to be in /upd folder
		if (f_stat("/upd/certs.bin", NULL) != FR_OK) {
			ESP_LOGE(TAG, "tried to update certs but no certs.bin");
			return;
		}

		ESP_LOGI(TAG, "Updating CA cert archive");

		FIL caF; f_open(&caF, "/upd/certs.bin", FA_READ);
		uint8_t magic[4];
		UINT br;

		if (f_read(&caF, magic, sizeof(magic), &br) != FR_OK || br != 4 || memcmp(magic, "MSCA", 4)) {
			f_close(&caF);
			ESP_LOGE(TAG, "invalid certs.bin");
			return;
		}

		uint32_t files_left;
		f_read(&caF, &files_left, 4, &br);

		char target_filename[64 + 4 + 1] { "/ca/" };

		while (files_left--) {
			// Read filename
			f_read(&caF, target_filename + 4, 64, &br);
			if (br != 64) {
				f_close(&caF);
				return;
			}
			uint32_t file_size;
			f_read(&caF, &file_size, 4, &br);
			if (br != 4) {
				f_close(&caF);
				return;
			}

			// If file exists, skip it
			if (f_stat(target_filename, NULL) == FR_OK) {
				ESP_LOGI(TAG, "File %s already exists, skipping", target_filename);
				f_lseek(&caF, f_tell(&caF) + file_size);
				continue;
			}

			ESP_LOGI(TAG, "Installing new cert %s", target_filename);
			if (!copy_updated_file(target_filename, &caF, (int)file_size)) {
				f_close(&caF);
				return;
			}
		}

		f_close(&caF);

		f_unlink("/upd/state");
		f_unlink("/upd/certs.bin");
		f_rmdir("/upd");

		return;
	}
}
