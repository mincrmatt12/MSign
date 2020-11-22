#ifndef TDSB_S_H
#define TDSB_S_H

#include "base.h"
#include <stdint.h>

namespace screen {
	struct TDSBScreen : public Screen {
		TDSBScreen();
		~TDSBScreen();

		static void prepare(bool);

		void draw();

	private:
		// draw the no school bed and text in the given region (cutting out the "noschool" line if required)
		void draw_noschool(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

		enum SegmentType {
			AsynchronousSchool,
			SynchronousSchool,
			InPersonSchool
		};

		// Draw a class segment on the main screen
		void draw_current_segment(int16_t y, SegmentType type, const uint8_t * course_code, const uint8_t * course_title, const uint8_t * teacher_name, const uint8_t * room_text=nullptr);

		// Draw a class segment on the nextday screen
		void draw_next_segment(int16_t y, SegmentType type);

		uint16_t colorbeat[2][3]{};
		uint64_t last_colorbeat;
	};
}

#endif
