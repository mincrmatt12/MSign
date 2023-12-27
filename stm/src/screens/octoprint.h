#ifndef OCTOPRINT_H
#define OCTOPRINT_H

#include "base.h"
#include <stdint.h>
#include "../common/slots.h"
#include "../common/bheap.h"
#include "../color.h"

namespace screen {
	struct OctoprintScreen : public Screen {
		OctoprintScreen();
		~OctoprintScreen();

		static void prepare(bool);

		void draw();
		void refresh();

	private:
	
		// all draw routines assume the servicer lock is held

		void draw_disabled_background(const char *text);
		void draw_background();
		void draw_gcode_progressbar(int8_t progress);
		void fill_gcode_area(const bheap::TypedBlock<uint8_t *>& bitmap, const slots::PrinterBitmapInfo& binfo, 
							 int x0, int y0, int x1, int y1, bool rotate_90, led::color_t filament);
	};
}

#endif
