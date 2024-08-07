#include <stdint.h>
#include "../config.h"

namespace cfgpull {
	//!cfg: holds .cfgpull.enabled, default false
	extern bool enabled;

	//!cfg: holds .cfgpull.only_firm, default false
	extern bool only_firm;

	//!cfg: holds .cfgpull.host
	extern config::string_t pull_host;

	//!cfg: holds .cfgpull.secret
	extern config::string_t pull_secret;

	//!cfg: holds .cfgpull.version, default 0
	extern int32_t version_id;
}
