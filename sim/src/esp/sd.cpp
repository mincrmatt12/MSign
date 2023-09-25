#include "sd.h"

#include <fcntl.h>
#include <fstream>
#include <ff.h>
#include <esp_log.h>
#include <diskio.h>
#include <iostream>
#include <filesystem>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

const static char * TAG = "diskio_fake_sd";

namespace sd {
	FATFS fs;

	struct mmap_holder {
		mmap_holder() {}

		mmap_holder(const mmap_holder&) = delete;
		mmap_holder(mmap_holder&&) = delete;

		~mmap_holder() {
			if (mapping_base) {
				munmap(mapping_base, sector_count * 512);
				mapping_base = nullptr;
			}
		}

		void init() {
			if (mapping_base) return;
			sector_count = std::filesystem::file_size("sdcard.bin") / 512;

			int fd = open("sdcard.bin", O_RDWR);
			if (fd >= 0) {
				mapping_base = mmap(nullptr, sector_count * 512, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
				if (mapping_base == MAP_FAILED) throw std::runtime_error("failed to map");
				close(fd);
			}
			else {
				throw std::runtime_error("failed to open sdcard");
			}
		}
		void * get_sector_address(LBA_t sector) {
			return (void *)((uintptr_t)mapping_base + sector * 512);
		}

		explicit operator bool() const {
			return mapping_base != nullptr;
		}

		size_t get_sector_count() {
			return sector_count;
		}

	private:
		void * mapping_base = nullptr;
		size_t sector_count = 0;
	};

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

sd::mmap_holder mmap_holder;

extern "C" DSTATUS disk_initialize(BYTE) {
	mmap_holder.init();
	return 0;
}

extern "C" DSTATUS disk_status(BYTE) {
	if (mmap_holder) return 0;
	return STA_NOINIT;
}

extern "C" DRESULT disk_write(BYTE, const BYTE* buff, LBA_t sec, UINT count) {
	if (!mmap_holder) return RES_NOTRDY;
	memcpy(mmap_holder.get_sector_address(sec), buff, count*512);
	return RES_OK;
}

extern "C" DRESULT disk_read(BYTE, BYTE* buff, LBA_t sec, UINT count) {
	if (!mmap_holder) return RES_NOTRDY;
	memcpy(buff, mmap_holder.get_sector_address(sec), count*512);
	return RES_OK;
}

extern "C" DRESULT disk_ioctl(BYTE, BYTE cmd, void *buff) {
	if (!mmap_holder) return RES_NOTRDY;
	switch (cmd) {
		case CTRL_SYNC:
			return RES_OK;
		case GET_SECTOR_COUNT:
			*((DWORD *) buff) = mmap_holder.get_sector_count();
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
