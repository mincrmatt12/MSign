#ifndef MSN_CRASH_SRVD
#define MSN_CRASH_SRVD

#include "FreeRTOS.h"
#include "../srv.h"

namespace crash::srvd {
	struct ServicerDebugAccessor {
		static const StaticQueue_t * get_debug_servicer_pending_requests();
		static bool get_debug_servicer_did_ping();
	};
}

#endif
