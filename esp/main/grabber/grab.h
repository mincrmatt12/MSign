#ifndef MSN_GRAB_H
#define MSN_GRAB_H

#include <FreeRTOS.h>

namespace grabber {
	// Start a-grabbin!
	void run(void*);

	struct Grabber {
		void (*init_func)(){};
		bool (*grab_func)(){}; // returns true for normal timeout, false for failure timeout

		TickType_t loop_time = pdMS_TO_TICKS(10000);
		TickType_t fail_time = pdMS_TO_TICKS(1000);

		bool ssl = false;
	};

	constexpr Grabber make_http_grabber(void (*_if)(), bool (*_gf)(), TickType_t loop, TickType_t fail=pdMS_TO_TICKS(5000)) {
		Grabber g;
		g.init_func = _if;
		g.grab_func = _gf;
		g.loop_time = loop;
		g.fail_time = fail;
		g.ssl = false;
		return g;
	}

	constexpr Grabber make_https_grabber(void (*_if)(), bool (*_gf)(), TickType_t loop, TickType_t fail=pdMS_TO_TICKS(8000)) {
		Grabber g;
		g.init_func = _if;
		g.grab_func = _gf;
		g.loop_time = loop;
		g.fail_time = fail;
		g.ssl = true;
		return g;
	}
}

#endif
