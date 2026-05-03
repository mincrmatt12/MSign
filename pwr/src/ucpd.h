#pragma once

#include "manual_box.h"

namespace ucpd {
	// runs as part of initializing power manager; sets Rd on the cc lines and disables pulldowns so
	// the usart init code can remap pa9/pa10
	//
	// incidentically, this also enables the ucpd bus
	void strobe_preinit();

	struct PortManager {
		PortManager();

		void run();
	};

	extern util::manual_box<PortManager> port;
}
