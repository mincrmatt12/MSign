#include "sd.h"

#include <fstream>
#include <ff.h>
#include <esp_log.h>
#include <diskio.h>
#include <iostream>
#include <filesystem>

const static char * TAG = "diskio_fake_sd";

namespace sd {
	FATFS fs;

	InitStatus init() {
		ESP_LOGI(TAG, "initing fake thingy");

		// Mount volume
		switch (f_mount(&sd::fs, "", 1)) {
			case FR_OK:
				break;
			case FR_NO_FILESYSTEM:
				ESP_LOGE(TAG, "No FAT detected");
				return InitStatus::FormatError;
			default:
				ESP_LOGE(TAG, "Unknown FatFS error");
				return InitStatus::UnkErr;
		}

		return InitStatus::Ok;
	}

	void flush_logs() {
		ESP_LOGD("fakesdlog", "would've flushed logs; sim");
	}

	void install_log() {
		ESP_LOGD("fakesdlog", "would've installed logs; sim");
	}
}

std::fstream sd_bigblob;

extern "C" DSTATUS disk_initialize(BYTE) {
	if (sd_bigblob.is_open()) return 0;

	// Try to open the sdcard.bin file
	sd_bigblob.open("sdcard.bin", std::ios::in|std::ios::out|std::ios::binary);
	if (!sd_bigblob.is_open()) return STA_NOINIT;
	return 0;
}

extern "C" DSTATUS disk_status(BYTE) {
	if (sd_bigblob.is_open()) return 0;
	return STA_NOINIT;
}

extern "C" DRESULT disk_write(BYTE, const BYTE* buff, LBA_t sec, UINT count) {
	if (!sd_bigblob.is_open()) return RES_NOTRDY;
	sd_bigblob.seekp(sec*512, std::ios_base::beg);
	sd_bigblob.write(reinterpret_cast<const char *>(buff), count*512);
	if (!sd_bigblob) return RES_ERROR;
	return RES_OK;
}

extern "C" DRESULT disk_read(BYTE, BYTE* buff, LBA_t sec, UINT count) {
	if (!sd_bigblob.is_open()) return RES_NOTRDY;
	sd_bigblob.seekg(sec*512, std::ios_base::beg);
	sd_bigblob.read(reinterpret_cast<char *>(buff), count*512);
	if (!sd_bigblob) return RES_ERROR;
	return RES_OK;
}

extern "C" DRESULT disk_ioctl(BYTE, BYTE cmd, void *buff) {
	if (!sd_bigblob.is_open()) return RES_NOTRDY;
	switch (cmd) {
		case CTRL_SYNC:
			sd_bigblob.sync();
			return RES_OK;
		case GET_SECTOR_COUNT:
			*((DWORD *) buff) = std::filesystem::file_size(std::filesystem::current_path() / "sdcard.bin") / 512;
			return RES_OK;
		case GET_SECTOR_SIZE:
			*((DWORD *) buff) = 512;
			return RES_OK;
		case GET_BLOCK_SIZE:
			return RES_ERROR;
		default:
			return RES_PARERR;
	}
}
