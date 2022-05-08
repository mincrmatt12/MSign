#ifndef PARCELS_H
#define PARCELS_H

#include "base.h"
#include <stdint.h>
#include "../draw.h"

#include "../common/slots.h"

namespace screen {
	struct ParcelScreen : public Screen {
		ParcelScreen();
		~ParcelScreen();

		static void prepare(bool);

		void draw();
		void refresh();
	private:
		int16_t draw_short_parcel_entry(int16_t y, const slots::ParcelInfo& info);

		// Scroll tracker for parcels
		constexpr static draw::PageScrollHelper::Params scroll_params = {
			.threshold_screen_start = 0,
			.hold_time = 2500
		};

		draw::PageScrollHelper scroll_tracker{scroll_params};
	};
}

#endif
