#ifndef MSS_DEBUG_H
#define MSS_DEBUG_H

#include "schedef.h"
#include <stdint.h>

namespace tasks {
	struct DebugConsole : public sched::Task {
		DebugConsole() {
			name[0] = 'd';
			name[1] = 'b';
			name[2] = 'g';
			name[3] = 'c';
		}

		bool important() override;
		bool done() override;
		void loop() override;
	};
}

#endif
