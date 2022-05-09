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

		bool interact();
	private:
		int16_t draw_short_parcel_entry(int16_t y, const slots::ParcelInfo& info);
		void draw_long_view(const slots::ParcelInfo& info);

		led::color_t draw_parcel_name(int16_t y, const slots::ParcelInfo& psl);

		int16_t draw_long_parcel_entry(int16_t y, const slots::ParcelStatusLine& psl, const uint8_t * heap, size_t heap_size, uint64_t updated_time);

		// Scroll tracker for parcels
		constexpr static draw::PageScrollHelper::Params scroll_params = {
			.start_y = 0,
			.threshold_screen_start = 0,
			.hold_time = 2500
		};

		draw::PageScrollHelper scroll_tracker{scroll_params};

		// Subscreen state
		int selected_parcel = 0, parcel_entries_scroll = 0, parcel_entries_size = 0;

		enum Subscreen {
			SubscreenSelecting = 1,
			SubscreenLongView = 2
		} subscreen = SubscreenSelecting;
	};
}

#endif
