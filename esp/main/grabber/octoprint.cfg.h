#pragma once

#include "../config.h"

namespace octoprint {
	//!cfg: holds .octoprint.key
	extern config::string_t api_key;

	//!cfg: holds .octoprint.host
	extern config::string_t host;

	struct FilamentColor {
		//!cfg: holds .filament_color
		uint8_t color[3] = {0xff, 0x77, 0x10};
	};

	//!cfg: holds .octoprint
	extern FilamentColor filament_color;

	//!cfg: holds .octoprint.g_relative_is_e, default false
	extern bool g_relative_is_e;

	//!cfg: holds .octoprint.max_streaming_size, default -1
	extern int64_t max_streaming_size;

	//!cfg: holds .octoprint.filter_prefixes
	extern config::string_t filter_prefixes[3];

	//!cfg: holds .octoprint.max_layer_height, default 0.5
	extern float max_layer_height;
}
