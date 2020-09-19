#ifndef MSN_SCREEN_H
#define MSN_SCREEN_H

#include "../screens/base.h"

#include "../screens/ttc.h"

namespace tasks {
	struct DispMan {
		void run();

	private:
		screen::ScreenSwapper<
			screen::TTCScreen
		> swapper;
	};
}

#endif
