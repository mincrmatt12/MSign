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

		inline static bool require_clearing() { return false; }
	private:
	
		// all draw routines assume the servicer lock is held

		void draw_disabled_background(const char *text);
		void draw_background();
		void draw_gcode_progressbar(int8_t progress, bool has_download_phase);

		template<bool Rotate90>
		void fill_gcode_area_impl(const bheap::TypedBlock<uint8_t *>& bitmap, const slots::PrinterBitmapInfo& binfo, 
							 int x0, int y0, int x1, int y1, led::color_t filament);

		void fill_gcode_area(const bheap::TypedBlock<uint8_t *>& bitmap, const slots::PrinterBitmapInfo& binfo, 
							 int x0, int y0, int x1, int y1, bool rotate_90, led::color_t filament) {
			if (rotate_90)
				fill_gcode_area_impl<true>(bitmap, binfo, x0, y0, x1, y1, filament);
			else
				fill_gcode_area_impl<false>(bitmap, binfo, x0, y0, x1, y1, filament);
		}

		// Helpers for caching

		int last_x0, last_x1, last_y0, last_y1;
		bool has_valid_bitmap_drawn = false;
	};
}

#endif
