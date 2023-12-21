#include "srvd.h"
#include "../srv.h"

extern srv::Servicer servicer;

const StaticQueue_t * crash::srvd::ServicerDebugAccessor::get_debug_servicer_pending_requests() {
	return &servicer.pending_requests_private;
}

bool crash::srvd::ServicerDebugAccessor::get_debug_servicer_did_ping() {
	return servicer.sent_ping;
}
