#include "modelserve.h"
#include "../config.h"
#include "../serial.h"
#include "../common/slots.h"
#include <cmath>
#include <limits>
#include "esp_log.h"
#include "ff.h"
#include "modelserve.cfg.h"

const static char * TAG = "modelserve";

namespace modelserve {
	uint8_t  modelidx = 0;
	time_t last_switch_time = 0;

	bool     modelspresent[3] = {false, false, true};

	const char * const modelpaths[] = {
		"/model.bin",
		"/model1.bin"
	};

	void update_modelpresence(size_t n, bool value) {
		if (n < 2 && f_stat(modelpaths[n], NULL)) {
			ESP_LOGW(TAG, "No model bin on disk but it was requested, cancelling (%d)", (int)n);
			modelspresent[n] = false;
			return;
		}
		modelspresent[n] = value;
	}

	float get_token_value(const char * v, int i) {
		char * temp = strdup(v);
		char * token = strtok(temp, ",");
		for (;i > 0;--i) token = strtok(nullptr, ",");
		if (token == nullptr) {
			free(temp);
			return std::numeric_limits<float>::quiet_NaN();
		}
		float r = atof(token);
		free(temp);
		return r;
	}

	void send_model_parameters() {
		if (modelidx == 2) return;
		
		serial::interface.update_slot_raw(slots::MODEL_CAM_FOCUS, model_params[modelidx].focuses, sizeof(slots::Vec3)*model_params[modelidx].nfocus, false);
		serial::interface.update_slot_nosync(slots::MODEL_CAM_MINPOS, model_params[modelidx].minpos);
		serial::interface.update_slot_nosync(slots::MODEL_CAM_MAXPOS, model_params[modelidx].maxpos);
	}

	void send_model_data() {
		// Create blocks of up to 12 triangles.
		slots::Tri chunk[12] {};
		float buf[3];
		FIL f;

		// Empty pre-existing slot buffer
		serial::interface.delete_slot(slots::MODEL_DATA);

		if (f_open(&f, modelpaths[modelidx], FA_READ) != FR_OK) {
			ESP_LOGE(TAG, "Failed to open model for send");
			modelspresent[modelidx] = false;
			return;
		}

		uint16_t tricount;
		UINT x;
		f_read(&f, &tricount, 2, &x);

		if (tricount == 0) return;

		for (int index = 0; index < tricount; index += 12) {
			size_t remaining_triangles = std::min(tricount - index, 12);

			// Allocate chunk
			serial::interface.allocate_slot_size(slots::MODEL_DATA, (index + remaining_triangles)*sizeof(slots::Tri));

			for (int triangle = 0; triangle < remaining_triangles; ++triangle) {
				f_read(&f, &buf[0], sizeof(buf), &x); // reads p1
				chunk[triangle].p1.set_x(buf[0]);
				chunk[triangle].p1.set_y(buf[1]);
				chunk[triangle].p1.set_z(buf[2]);
				f_read(&f, &buf[0], sizeof(buf), &x); // reads p2
				chunk[triangle].p2.set_x(buf[0]);
				chunk[triangle].p2.set_y(buf[1]);
				chunk[triangle].p2.set_z(buf[2]);
				f_read(&f, &buf[0], sizeof(buf), &x); // reads p3
				chunk[triangle].p3.set_x(buf[0]);
				chunk[triangle].p3.set_y(buf[1]);
				chunk[triangle].p3.set_z(buf[2]);
				f_read(&f, &buf[0], sizeof(buf), &x); // reads rgb
				chunk[triangle].r = buf[0];
				chunk[triangle].g = buf[1];
				chunk[triangle].b = buf[2];
				chunk[triangle].pad = 0;
			}

			// Send chunk
			serial::interface.update_slot_range(slots::MODEL_DATA, chunk, index, remaining_triangles);
		}

		f_close(&f);
		slots::ModelInfo mi;
		mi.tri_count = tricount;
		mi.use_lighting = model_params[modelidx].lighting_mode == ModelParams::DYNAMIC;
		// Finally update INFO
		serial::interface.update_slot(slots::MODEL_INFO, mi);
	}

	void init_load_model(int i) {
		modelidx = i;
		if (i < 2) {
			// Send model parameters to device
			send_model_parameters();
			// Send all triangles
			send_model_data();
		}
		else {
			slots::ModelInfo mi;
			mi.tri_count = 0;
			serial::interface.update_slot_nosync(slots::MODEL_INFO, mi);
			serial::interface.delete_slot(slots::MODEL_DATA);
			serial::interface.sync();
		}
	}

	void init() {
		// init() is called after config is loaded so this will work correctly.
		modelidx = 2;
		ESP_LOGI(TAG, "Initialized model information");
	}

	bool loop() {
		++modelidx;
		int last_idx = modelidx;

		for (int i = 0; i < 3; ++i) {
			++modelidx;
			modelidx %= 3;
			if (modelspresent[modelidx]) break;
		}

		if (modelidx == last_idx) return true;
		init_load_model(modelidx);
		return true;
	}
}
