#include "modelserve.h"
#include "../config.h"
#include "../serial.h"
#include "../common/slots.h"
#include <cmath>
#include <limits>
#include "esp_log.h"
#include "ff.h"

const static char * TAG = "modelserve";

namespace modelserve {
	uint8_t  modelidx = 0;
	time_t last_switch_time = 0;

	bool     modelspresent[3] = {false, false, true};

	const char * modelpaths[] = {
		"/model.bin",
		"/model1.bin"
	};

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

	void send_model_parameters(config::Entry e) {
		if (modelidx == 2) return;
		slots::Vec3 v;
		const char * c;
		if (e == config::MODEL_FOCUSES && (c = config::manager.get_value(config::MODEL_FOCUSES))) {
			v.x = get_token_value(c, modelidx * 9 + 0);
			v.y = get_token_value(c, modelidx * 9 + 1);
			v.z = get_token_value(c, modelidx * 9 + 2);

			serial::interface.update_slot(slots::MODEL_CAM_FOCUS1, v);

			float temp = get_token_value(c, modelidx * 9 + 3);
			if (!std::isnan(temp)) {
				v.x = temp;
				v.y = get_token_value(c, modelidx * 9 + 4);
				v.z = get_token_value(c, modelidx * 9 + 5);
			}

			serial::interface.update_slot(slots::MODEL_CAM_FOCUS2, v);

			temp = get_token_value(c, modelidx * 9 + 6);
			if (!std::isnan(temp)) {
				v.x = temp;
				v.y = get_token_value(c, modelidx * 9 + 4);
				v.z = get_token_value(c, modelidx * 9 + 5);
			}

			v.y = get_token_value(c, modelidx * 9 + 7);
			v.z = get_token_value(c, modelidx * 9 + 8);

			serial::interface.update_slot(slots::MODEL_CAM_FOCUS3, v);
		}
		else if (e == config::MODEL_MINPOSES && (c = config::manager.get_value(config::MODEL_MINPOSES))) {
			v.x = get_token_value(c, modelidx * 3 + 0);
			v.y = get_token_value(c, modelidx * 3 + 1);
			v.z = get_token_value(c, modelidx * 3 + 2);

			serial::interface.update_slot(slots::MODEL_CAM_MINPOS, v);
		}
		else if (e == config::MODEL_MAXPOSES && (c = config::manager.get_value(config::MODEL_MAXPOSES))) {
			v.x = get_token_value(c, modelidx * 3 + 0);
			v.y = get_token_value(c, modelidx * 3 + 1);
			v.z = get_token_value(c, modelidx * 3 + 2);

			serial::interface.update_slot(slots::MODEL_CAM_MAXPOS, v);
		}
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
				chunk[triangle].p1.x = buf[0];
				chunk[triangle].p1.y = buf[1];
				chunk[triangle].p1.z = buf[2];
				f_read(&f, &buf[0], sizeof(buf), &x); // reads p2
				chunk[triangle].p2.x = buf[0];
				chunk[triangle].p2.y = buf[1];
				chunk[triangle].p2.z = buf[2];
				f_read(&f, &buf[0], sizeof(buf), &x); // reads p3
				chunk[triangle].p3.x = buf[0];
				chunk[triangle].p3.y = buf[1];
				chunk[triangle].p3.z = buf[2];
				f_read(&f, &buf[0], sizeof(buf), &x); // reads rgb
				chunk[triangle].r = buf[0];
				chunk[triangle].g = buf[1];
				chunk[triangle].b = buf[2];
				chunk[triangle].pad = 0;
			}

			// Send chunk
			serial::interface.update_slot_partial(slots::MODEL_DATA, index*sizeof(slots::Tri), chunk, remaining_triangles*sizeof(slots::Tri));
		}

		f_close(&f);
		slots::ModelInfo mi;
		mi.tri_count = tricount;
		mi.use_lighting = true;
		// Finally update INFO
		serial::interface.update_slot(slots::MODEL_INFO, mi);
	}

	void init_load_model(int i) {
		modelidx = i;
		if (i < 2) {
			// Send model parameters to device
			send_model_parameters(config::MODEL_FOCUSES);
			send_model_parameters(config::MODEL_MINPOSES);
			send_model_parameters(config::MODEL_MAXPOSES);
			// Send all triangles
			send_model_data();
		}
		else {
			slots::ModelInfo mi;
			mi.tri_count = 0;
			serial::interface.update_slot(slots::MODEL_INFO, mi);
			serial::interface.delete_slot(slots::MODEL_DATA);
		}
	}

	bool init_modeldat(int i) {
		if (f_stat(modelpaths[i], NULL)) {
			ESP_LOGW(TAG, "No model bin on disk (%d)", i);
			modelspresent[i] = false;

			return false;
		}

		ESP_LOGI(TAG, "Got model of for (%d)", i);
		modelspresent[i] = true;

		return true;
	}

	void init() {
		init_modeldat(0);
		init_modeldat(1);

		// check present data
		char * temp = strdup(config::manager.get_value(config::MODEL_ENABLE, "0,0,1"));
		char * token = temp;
		for (int i = 0; i < 3; ++i) {
			token = strtok(i == 0 ? temp : NULL, ",");
			if (*token == '0') modelspresent[i] = false;
		}
		free(temp);

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
