//!cfg: include ../common/slots.h
#include "../common/slots.h"

namespace modelserve {
	struct ModelParams {
		//!cfg: holds .cam.focuses
		slots::Vec3 focuses[3];

		//!cfg: holds .cam.min
		slots::Vec3 minpos;

		//!cfg: holds .cam.max
		slots::Vec3 maxpos;

		enum LightingMode : uint8_t {
			BAKED,
			DYNAMIC
		};

		//!cfg: holds .lighting
		LightingMode lighting_mode = DYNAMIC;

		//!cfg: holds .cam.focuses!size
		uint8_t nfocus = 0;
	};

	//!cfg: holds .model.sd
	extern ModelParams model_params[2];

	//!cfg: receives .model.enabled[$n], max $n=3
	void update_modelpresence(size_t n, bool value);

	//!cfg: receives .model.sd[$n].name
	inline void update_modelname(size_t n, const char *value) {}
}
