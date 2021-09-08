#ifndef TTC_H
#define TTC_H

#include "base.h"
#include <stdint.h>

#include "../common/bheap.h"

namespace screen {
	struct TTCScreen : public Screen {
		TTCScreen();
		~TTCScreen();

		static void prepare(bool);

		void draw();
		void refresh();
	private:
		void draw_bus();
		// Draw an entire slot 
		int16_t draw_slot(uint16_t y, const uint8_t * name, const bheap::TypedBlock<uint64_t *> &times_a, const bheap::TypedBlock<uint64_t *> &times_b, bool alert, bool delay, char a_code, char b_code);
		// Helper routine, draws (if data available) a subrow
		bool draw_subslot(uint16_t y, char dircode, const bheap::TypedBlock<uint64_t *> &times);
		void draw_alertstr();

		static void request_slots(uint32_t temp);

		uint8_t bus_type = 1;
		uint8_t bus_state = 0;

		int16_t total_current_height = -1, scroll_offset = 0, scroll_target = 0;
		TickType_t last_scrolled_at;
	};
}

#endif
