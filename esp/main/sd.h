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

	extern FATFS fs;
}

#endif
