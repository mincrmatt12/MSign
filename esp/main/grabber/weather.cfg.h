#include "../config.h"

namespace weather {
	//!cfg: holds .weather.key
	extern config::string_t api_key;

	//!cfg: holds .weather.coord[0]
	extern float lat;

	//!cfg: holds .weather.coord[1]
	extern float lon;
}
