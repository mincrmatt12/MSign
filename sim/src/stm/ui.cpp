#include "ui.h"
#include "srv.h"

extern srv::Servicer servicer;

ui::Buttons ui::buttons;

void ui::Buttons::init() {
	if (servicer.ready()) {
		// Ask for button map to be made warm for remote control.
		servicer.set_temperature(slots::VIRTUAL_BUTTONMAP, bheap::Block::TemperatureWarm);
	}
}

void ui::Buttons::update() {
	if (!last_update) last_update = xTaskGetTickCount();
	auto now = xTaskGetTickCount();
	TickType_t duration = now - last_update;
	if (duration) last_update = now;

	last_held = current_held;
	current_held = 0;

	if (servicer.ready()) {
		srv::ServicerLockGuard g(servicer);

		auto& blk = servicer.slot<uint16_t>(slots::VIRTUAL_BUTTONMAP);
		if (blk && blk.datasize == 2) current_held |= *blk;
	}

	for (int i = 0; i < 11; ++i) {
		if (current_held & (1 << i)) {
			held_duration[i] += duration;
		}
		else held_duration[i] = 0;
	}
}

bool ui::Buttons::held(Button b, TickType_t mindur) const {
	return current_held & (1 << b) && held_duration[b] >= mindur;
}

bool ui::Buttons::rel(Button b) const {
	return !held(b) && (current_held ^ last_held) & (1 << b);
}

bool ui::Buttons::press(Button b) const {
	return held(b) && (current_held ^ last_held) & (1 << b);
}

bool ui::Buttons::changed() const {
	return current_held ^ last_held;
}
