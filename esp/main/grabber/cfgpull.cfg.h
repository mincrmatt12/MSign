#include <stdint.h>
#include "../config.h"

namespace cfgpull {
	//!cfg: holds .cfgpull.enabled, default false
	extern bool enabled;

	//!cfg: holds .cfgpull.host
	extern const char * pull_host;

	//!cfg: holds .cfgpull.secret
	extern const char * pull_secret;

	//!cfg: holds .cfgpull.version, default 0
	extern int32_t version_id;
}
