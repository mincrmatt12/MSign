#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <timers.h>
#include <string.h>
#include "esp_log.h"
#include "sd.h"
#include <ff.h>
#include <stdio.h>

static const char * const TAG = "sdlog";

namespace sd {
	struct LogBuf {
		static const size_t Len = 1024;

		size_t pos = 0;
		volatile size_t lendpos = 0;
		size_t end = 0;

		char buf[Len];

		volatile bool in_use = false;
		volatile bool lending = false;

		bool mask = false;
		bool ovf = false;

		void putc(char c) {
			if (mask) {
				if (c == 'm') mask = false;
				return;
			}
			if (c == 0x1b) {
				mask = true;
				return;
			}

			if (in_use)
				return;

			bool lending_ = lending;

			if (!lending_ && end != lendpos) {
				memmove(buf, buf + lendpos, end - lendpos);

				pos = end - lendpos;
				lendpos = end = 0;
			}

			size_t& pos_ = lending_ ? end : pos;

			if (pos_ == Len) {
				ovf = true;
				return;
			}
			else {
				buf[pos_++] = c;
			}
		}

		bool pend() const {
			return pos > 0 || ovf;
		}

		FRESULT dump(FIL* fp) {
			if (!lending && end != lendpos)
				return FR_OK;
			portENTER_CRITICAL();
			in_use = true;
			lending = true;
			lendpos = end = pos;
			in_use = false;
			portEXIT_CRITICAL();
			UINT bw = 0;
			while (bw < pos) {
				UINT bw_ = 0;
				auto res = f_write(fp, buf + bw, pos - bw, &bw_);
				bw += bw_;
				if (res != FR_OK) {
					return clear(res);
				}
			}
			if (ovf) {
				ovf = false;
				f_putc('\n', fp);
			}
			return clear(FR_OK);
		}

		void clear() {
			clear(FR_OK);
		}

	private:
		FRESULT clear(FRESULT passthrough) {
			portENTER_CRITICAL();
			in_use = true;
			pos = 0;
			ovf = false;
			lending = false;
			in_use = false;
			portEXIT_CRITICAL();
			return passthrough;
		}
	} logbuf;

	void flush_logs(TimerHandle_t) {
		if (!logbuf.pend()) return;

		// logs available to flush
		FIL logfil;
		if (f_open(&logfil, "/log/log.0", FA_OPEN_APPEND | FA_WRITE) != FR_OK) {
			logbuf.clear();
			return;
		}

		logbuf.dump(&logfil);

		f_sync(&logfil);
		f_close(&logfil);
	}

	void refresh_log_dir(const char *prefix) {
		f_mkdir("/log");
		// Try and rotate log files, storing up to 10 previous entries
		DIR logdir; FILINFO fno;
		int maxn = 0;
		f_opendir(&logdir, "/log");
		char scanstr[10]; snprintf(scanstr, sizeof scanstr, "%s.%%d", prefix);
		while (f_readdir(&logdir, &fno) == FR_OK && fno.fname[0] != 0) {
			int num;
			if (sscanf(fno.fname, scanstr, &num) != 1) continue;
			if (num > maxn) maxn = num;
		}
		f_closedir(&logdir);
		// rename files
		for (int i = maxn; i >= 0; --i) {
			char oldname[32], newname[32];
			snprintf(oldname, 32, "/log/%s.%d", prefix, i);
			snprintf(newname, 32, "/log/%s.%d", prefix, i+1);
			if (i > 10) {
				ESP_LOGW("sdlog", "deleting old %s", oldname);
				f_unlink(oldname);
			}
			else {
				ESP_LOGI("sdlog", "moving old %s -> %s", oldname, newname);
				f_rename(oldname, newname);
			}
		}
	}

	void init_logger() {
		refresh_log_dir();
		f_unlink("/log/log.0");
		// Start timer
		auto tim = xTimerCreate("logf", pdMS_TO_TICKS(360), true, nullptr, flush_logs);
		if (tim == NULL) {
			ESP_LOGE(TAG, "failed to start log flusher task");
			return;
		}
		xTimerStart(tim, pdMS_TO_TICKS(1000));
	}

	void log_putc(char c) {
		logbuf.putc(c);
	}

	size_t copy_pending_logs(char *buf, size_t max) {
		if (max > logbuf.pos)
			max = logbuf.pos;

		memcpy(buf, logbuf.buf, max);

		return max;
	}
}
