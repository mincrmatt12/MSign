#ifndef MSN_SD_H
#define MSN_SD_H

#include <ff.h>
#include <FreeRTOS.h>
#include <stream_buffer.h>

namespace sd {
	enum struct InitStatus {
		Ok,
		NoCard,
		FormatError,
		UnusableCard,
		UnkErr
	};

	// Sets up FatFS with this.
	InitStatus init();
	void init_logger();

	void log_putc(char c);

	extern FATFS fs;
}

#endif
