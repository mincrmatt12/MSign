#include "upd.h"
#include <cstring>
#include <esp_log.h>
#include "config.h"
#include "common/util.h"
#include "esp_system.h"
#include "sd.h"
#include "uart.h"
#include "common/slots.h"
#include <FreeRTOS.h>
#include <task.h>

#include <esp_partition.h>
#include <esp_ota_ops.h>

#define USTATE_NOT_READ 0xff
#define USTATE_NONE 0xfe

#define USTATE_READY_TO_START 0x00
#define USTATE_CURRENT_SENDING_STM 0x01
#define USTATE_WAITING_FOR_FINISH 0x02
#define USTATE_READY_TO_DO_ESP 0x03
#define USTATE_JUST_DID_ESP 0x05

#define USTATE_READY_FOR_WEB 0x10

#define USTATE_READY_FOR_CERT 0x20

const static char * TAG="updater";

namespace upd {
	uint8_t update_state = 0xff;


	UpdateKind needed() {
		if (update_state != USTATE_NOT_READ) {
			goto compute;
		}
		// check the SD for presence of upd/state file
		if (f_stat("/upd/state", NULL) == FR_OK) {
			ESP_LOGI(TAG, "update file exists...");
			FIL f; f_open(&f, "/upd/state", FA_READ);
			UINT br;
			f_read(&f, &update_state, 1, &br);
			if (br) {
				ESP_LOGD(TAG, "got update w/ code %02x", update_state);
			}
			else {
				ESP_LOGE(TAG, "empty or errored update file");
				update_state = USTATE_NONE;
			}
			f_close(&f);
		}
		else {
			update_state = USTATE_NONE;
		}
	compute:
		switch (update_state) {
			case USTATE_READY_FOR_WEB:
				return upd::WEB_UI;
			case USTATE_READY_FOR_CERT:
				return upd::CERTIFICATE_FILE;
			case USTATE_NONE:
				return upd::NO_UPDATE;
			default:
				return upd::FULL_SYSTEM;
		}
	}

	void update_website() {
		// check for the webui.ar file
		if (f_stat("/upd/webui.ar", NULL) != FR_OK) {
			ESP_LOGD(TAG, "tried to update web but no webui.ar file");
			return;
		}

		// otherwise, prepare by BLOWING THE OLD SHIT TO LITTE BITS
		
		ESP_LOGD(TAG, "removing old files...");

		f_unlink("/web/page.html");
		f_unlink("/web/page.css");
		f_unlink("/web/page.js");
		f_unlink("/web/page.html.gz");
		f_unlink("/web/page.css.gz");
		f_unlink("/web/page.js.gz");
		f_unlink("/web/etag"); // ignore failures here

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

		while (files_read < 6) {
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

			int iH = 0, iC = 0, iJ = 0, iHc = 0, iCc = 0, iJc = 0;
			// check the filename
			if ((iH = memcmp(fileHeader, "page.html/", 10))
				&& (iC = memcmp(fileHeader, "page.css/", 9)) && (iJ = memcmp(fileHeader, "page.js/ ", 9)) && 
				(iHc = memcmp(fileHeader, "page.html.gz", 12))
				&& (iCc = memcmp(fileHeader, "page.css.gz/", 11)) && (iJc = memcmp(fileHeader, "page.js.gz/", 10))) {
				ESP_LOGD(TAG, "unknown entry");
				f_close(&arF);
				
				goto failure;
			}

			// ok, now get the size
			int length;
			if (1 != sscanf((char *)(fileHeader + 48), "%d", &length) || !length) {
				goto invalidformat;
			}

			const char * tfname;
			if (!iH) tfname = "/web/page.html";
			else if (!iC) tfname = "/web/page.css";
			else if (!iJ) tfname = "/web/page.js";
			else if (!iHc) tfname = "/web/page.html.gz";
			else if (!iCc) tfname = "/web/page.css.gz";
			else if (!iJc) tfname = "/web/page.js.gz";
			else goto failure;

			FIL target; f_open(&target, tfname, FA_WRITE | FA_CREATE_ALWAYS);

			int bytes_since_last_yield = 0;

			ESP_LOGI(TAG, "updating: %s", tfname);

			{ 
				uint8_t buf[256];
				for (int i = 0; i < length; i += 256) {
					int size_of_tx = (length - i >= 256) ? 256 : (length - i);
					if (f_eof(&arF)) {
						f_close(&target);
						f_close(&arF);
						ESP_LOGE(TAG, "failed to copy");
						goto failure;
					}
					f_read(&arF, buf, size_of_tx, &br);
					UINT bw;
					f_write(&target, buf, br, &bw);
					bytes_since_last_yield += bw;
					if (bytes_since_last_yield > 4096) {
						vTaskDelay(10);
						bytes_since_last_yield = 0;
					}
				}
			}

			f_close(&target);
			++files_read;
		}
		f_close(&arF);

		ESP_LOGI(TAG, "finished ui update.");
failure: // clean up
		f_unlink("/upd/state");
		f_unlink("/upd/webui.ar");
		f_rmdir("/upd");
	}

	void update_cacerts() {
		// TODO: make me extract the AR file.

		return;
	}

	// routines for doing update
	void send_update_chunk(uint8_t * buffer, size_t length) {
		if (length > 253) {
			ESP_LOGE(TAG, "invalid chunk length > 253");
		}

		uint16_t chksum = util::compute_crc(buffer, length);

		uint8_t send_buffer[5];
		send_buffer[0] = 0xa6;
		send_buffer[1] = length + 2;
		send_buffer[2] = slots::protocol::UPDATE_IMG_DATA;

		memcpy(&send_buffer[3], &chksum, 2);
		uart_write_bytes(UART_NUM_0, (char*)send_buffer, 5);
		uart_write_bytes(UART_NUM_0, (char*)buffer, length);
	}

	uint16_t send_update_start(uint16_t checksum, uint32_t length) {
		uint16_t chunk_count = (length / 253) + (length % 253 != 0);

		uint8_t send_buffer[11] = {
			0xa6,
			0x08,
			slots::protocol::UPDATE_IMG_START,
			0, 0, 0, 0, 0, 0, 0, 0
		};

		memcpy(&send_buffer[3], &checksum, 2);
		memcpy(&send_buffer[5], &length, 4);
		memcpy(&send_buffer[9], &chunk_count, 2);

		uart_write_bytes(UART_NUM_0, (char*)send_buffer, sizeof(send_buffer));
		return chunk_count;
	}

	void send_update_status(slots::protocol::UpdateCmd status) {
		uint8_t send_buffer[4] = {
			0xa6,
			0x01,
			slots::protocol::UPDATE_CMD,
			(uint8_t)status
		};

		uart_write_bytes(UART_NUM_0, (char*)send_buffer, sizeof(send_buffer));
	}

	bool retrieve_update_status(slots::protocol::UpdateStatus& status_out) {
		uint8_t recv_buffer[4];
		uart_read_bytes(UART_NUM_0, recv_buffer, sizeof(recv_buffer), portMAX_DELAY);
		if (recv_buffer[0] == 0xa5 && recv_buffer[1] == 0x01 && recv_buffer[2] == 0x63) {
			status_out = (slots::protocol::UpdateStatus)recv_buffer[3];
			return true;
		}
		return false;
	}

	void set_update_state(uint8_t update_new_state) {
		FIL f; f_open(&f, "/upd/state", FA_WRITE | FA_CREATE_ALWAYS);
		f_putc(update_new_state, &f);
		f_close(&f);
	}


	// full system update

	void update_system() {
		// check the files are present
		uint16_t crc_esp, crc_stm;
		if (f_stat("/upd/stm.bin", NULL) || f_stat("/upd/chck.sum", NULL)) {
			ESP_LOGE(TAG, "missing files.");

			f_unlink("/upd/state");
			return;
		}

		{
			FIL f; f_open(&f, "/upd/chck.sum", FA_READ);
			UINT br;
			f_read(&f, &crc_esp, 2, &br);
			f_read(&f, &crc_stm, 2, &br);
			f_close(&f);
		}

		// setup uart
		{
			uart_config_t cfg;
			cfg.baud_rate = update_state == USTATE_READY_TO_START ? 115200 : 230400;
			cfg.data_bits = UART_DATA_8_BITS;
			cfg.stop_bits = UART_STOP_BITS_1;
			cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
			cfg.parity = UART_PARITY_EVEN;
			uart_param_config(UART_NUM_0, &cfg);

			ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
		}

		// check what to do based on the state
		switch (update_state) {
			case USTATE_CURRENT_SENDING_STM:
				{
					// send a reset code
					send_update_status(slots::protocol::UpdateCmd::CANCEL_UPDATE);

					// reset update state:
					set_update_state(USTATE_READY_TO_START);

					// restart
					esp_restart();
				}
				break; // ?
			case USTATE_READY_TO_START:
				{
					// wait for the incoming HANDSHAKE_INIT command
					uint8_t buf[4] = {0};
				try_again:
					uart_read_bytes(UART_NUM_0, buf, 3, portMAX_DELAY);
				check_again:
					if (buf[0] != 0xa5 || buf[1] != 0x00 || buf[2] != 0x10) {
						if (buf[1] == 0xa5) {
							// Offset
							buf[0] = buf[1];
							buf[1] = buf[2];
							uart_read_bytes(UART_NUM_0, &buf[2], 1, portMAX_DELAY);
							goto check_again;
						}
						else if (buf[2] == 0xa5) {
							// Offset
							buf[0] = buf[2];
							uart_read_bytes(UART_NUM_0, &buf[1], 2, portMAX_DELAY);
							goto check_again;
						}
						else goto try_again;
					}

					buf[0] = 0xa6;
					buf[2] = 0x13;

					uart_write_bytes(UART_NUM_0, (char*)buf, 3);

					// Change baudrate here
					uart_wait_tx_done(UART_NUM_0, portMAX_DELAY);
					uart_set_baudrate(UART_NUM_0, 230400);

					slots::protocol::UpdateStatus status;

					if (!retrieve_update_status(status)) goto try_again;

					ESP_LOGI(TAG, "Connected to STM32 for update.");

					set_update_state(USTATE_CURRENT_SENDING_STM);  // mark current state

					// tell to prepare for image
					send_update_status(slots::protocol::UpdateCmd::PREPARE_FOR_IMAGE); // preapre for image

					// wait for command
					while (!retrieve_update_status(status)) vTaskDelay(pdMS_TO_TICKS(5));

					if (status != slots::protocol::UpdateStatus::READY_FOR_IMAGE) {
						ESP_LOGE(TAG, "invalid command for readyimg");
						esp_restart();
					}

					// image is ready, send the image command
					
					FIL stm_firmware; f_open(&stm_firmware, "/upd/stm.bin", FA_READ);
					vTaskDelay(2);
					auto chunk = send_update_start(crc_stm, f_size(&stm_firmware));
					uint8_t current_chunk_buffer[253];
					uint8_t current_chunk_size = 0;
					ESP_LOGI(TAG, "Sending image to STM32 in %d chunks", chunk);
					UINT br;
					
					while (true) {
						while (!retrieve_update_status(status)) vTaskDelay(1);

						switch (status) {
							case slots::protocol::UpdateStatus::READY_FOR_IMAGE:
								continue;
							case slots::protocol::UpdateStatus::READY_FOR_CHUNK:
								{
									--chunk;

									memset(current_chunk_buffer, 0, 253);
									f_read(&stm_firmware, current_chunk_buffer, 253, &br);
									current_chunk_size = br;
									if (!(chunk % 20)) ESP_LOGD(TAG, "sending chunk %d", chunk);
									break;
								}
							case slots::protocol::UpdateStatus::RESEND_LAST_CHUNK_CSUM:
								{
									break;
								}
							case slots::protocol::UpdateStatus::ABORT_CSUM:
								{
									ESP_LOGD(TAG, "stm failed csum check, stopping update.");
									f_close(&stm_firmware);
									f_unlink("/upd/state");
									esp_restart();
								}
							case slots::protocol::UpdateStatus::BEGINNING_COPY:
								{
									goto loopover;
								}
							default:
								ESP_LOGW(TAG, "unknown status code %02x", (int)status);
								continue;
						}
						if (current_chunk_size == 0) break;

						// send the chunk
						send_update_chunk(current_chunk_buffer, current_chunk_size);
					}
	loopover:
				ESP_LOGD(TAG, "done.\nwaiting for finished update");
				f_close(&stm_firmware);

				while (!retrieve_update_status(status)) vTaskDelay(pdMS_TO_TICKS(50));

				if (status != slots::protocol::UpdateStatus::COPY_COMPLETED) {
					ESP_LOGD(TAG, "huh? ignoring invalid status.");
				}

				// setting update status accordingly and resetting
				if (!f_stat("/upd/esp.bin", NULL)) {
					set_update_state(USTATE_READY_TO_DO_ESP);
				}
				else {
					set_update_state(USTATE_JUST_DID_ESP);
				}
				esp_restart();
			}
		case USTATE_READY_TO_DO_ESP:
			{
				// comes back here after a restart
				ESP_LOGI(TAG, "Continuing update; flashing to ESP.");

				uint8_t data_buffer[256];
				bool has_finished = false;
				uint16_t calculated_csum = 0;
				UINT last_size;

				esp_ota_handle_t updater;

				auto part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
				if (!part) {
					ESP_LOGE(TAG, "missing partition for ota update; spinning");
					while (1) {vTaskDelay(5);}
				}

				FIL esp_firmware; f_open(&esp_firmware, "/upd/esp.bin", FA_READ);
				ESP_ERROR_CHECK(esp_ota_begin(part, f_size(&esp_firmware), &updater));

				int bytes_since_last_yield = 0;
				
				// write the contents of esp.bin to flash at _FS_start, taking advantage of the _Helpfully_ allocated symbols :)
				while (!has_finished) {
					send_update_status(slots::protocol::UpdateCmd::ESP_WROTE_SECTOR);
					memset(data_buffer, 0, 256);

					f_read(&esp_firmware, data_buffer, 256, &last_size);
					has_finished = f_eof(&esp_firmware) || last_size < 256;
					calculated_csum = util::compute_crc(data_buffer, last_size, calculated_csum);
					
					esp_ota_write(updater, data_buffer, last_size);
					bytes_since_last_yield += last_size;

					if (bytes_since_last_yield > 32768) {
						vTaskDelay(2);
						bytes_since_last_yield = 0;
					}
				}

				f_close(&esp_firmware);

				if (calculated_csum != crc_esp) {
					ESP_LOGW(TAG, "unexpected checkum -- this might fuck up");
				}

				ESP_ERROR_CHECK(esp_ota_end(updater));

				send_update_status(slots::protocol::UpdateCmd::ESP_COPYING);
				// THE FLASH... HAS.. BEEN WRITTEN
				ESP_LOGI(TAG, "Wrote image.");

				set_update_state(USTATE_JUST_DID_ESP);

				// send eboot command
				/*
				eboot_command ebcmd;
				ebcmd.action = ACTION_COPY_RAW;
				ebcmd.args[0] = 0x300000;
				ebcmd.args[1] = 0x00000;
				eboot_command_write(&ebcmd);
				*/

				ESP_LOGD(TAG, "setting ota_1 as boot image (will copy in bootloader)");
				esp_ota_set_boot_partition(part);
				ESP_LOGD(TAG, "restarting");
				esp_restart();
			}
		case USTATE_JUST_DID_ESP:
			{
				// update finished, returns here
				ESP_LOGI(TAG, "Just returned from update OK!");

				// clean up
				f_unlink("/upd/state");
				f_unlink("/upd/stm.bin");
				f_unlink("/upd/chck.sum");
				f_unlink("/upd/esp.bin");
				f_rmdir("/upd");

				vTaskDelay(pdMS_TO_TICKS(1000));

				// send command
				send_update_status(slots::protocol::UpdateCmd::UPDATE_COMPLETED_OK);
				send_update_status(slots::protocol::UpdateCmd::UPDATE_COMPLETED_OK);
				send_update_status(slots::protocol::UpdateCmd::UPDATE_COMPLETED_OK);

				// restart
				esp_restart();
			}
		}
	}
}
