#ifndef TTC_H
#define TTC_H

#include "base.h"
#include <stdint.h>
#include "../draw.h"

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
		int16_t draw_slot(uint16_t y, const uint8_t * name, const bheap::TypedBlock<uint64_t *> &times_a, const bheap::TypedBlock<uint64_t *> &times_b, bool alert, bool delay, char a_code, char b_code, int distance);
		// Helper routine, draws (if data available) a subrow
		bool draw_subslot(uint16_t y, char dircode, const bheap::TypedBlock<uint64_t *> &times, int distance);
		void draw_alertstr();

		static void request_slots(uint32_t temp);

		uint8_t bus_type = 1;
		uint8_t bus_state = 0;

		constexpr static draw::PageScrollHelper::Params scroll_params = {};
		draw::PageScrollHelper scroll_helper{scroll_params};
	};
}

#endif
