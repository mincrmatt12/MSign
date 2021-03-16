#ifndef MSN_SD_H
#define MSN_SD_H

#include <ff.h>

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

	// Try to install log
	void install_log();

	// Flush logs (call periodically)
	void flush_logs();

	extern FATFS fs;
}

#endif
