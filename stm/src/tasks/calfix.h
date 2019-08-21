#ifndef MSN_CALFIX
#define MSN_CALFIX

#include "schedef.h"
#include <stdint.h>

namespace tasks {

	struct CalfixScreen : public sched::Task, public sched::Screen {
		bool init() override;
		bool deinit() override;

		void loop() override;
		bool done() override {return true;}
	private:
		void show_noschool();

		void draw_header(uint8_t schoolday, bool abnormal);
		void draw_class(uint64_t clstime, const char * name, uint16_t room, uint16_t y_pos, uint8_t colors_idx);

		uint8_t /* slot ids */ s_info, s_c[4], s_prd[2];
		bool ready = false;
	};

}

#endif
