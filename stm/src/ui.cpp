#include "ui.h"

#include "pins.h"
#include "srv.h"
#include <stm32f2xx_ll_bus.h>
#include <stm32f2xx_ll_gpio.h>

extern srv::Servicer servicer;

ui::Buttons ui::buttons;

void ui::Buttons::init() {
	LL_AHB1_GRP1_EnableClock(UI_BUTTON_PERIPH);

	LL_GPIO_InitTypeDef it{};

	it.Mode = LL_GPIO_MODE_INPUT;
	it.Pull = LL_GPIO_PULL_DOWN;
	it.Speed = LL_GPIO_SPEED_FREQ_LOW;
	it.Pin = (1 << POWER) | (1 << NXT) | (1 << PRV) | (1 << SEL) | (1 << MENU);

	LL_GPIO_Init(UI_BUTTON_PORT, &it);
	
	if (servicer.ready()) {
		// Ask for button map to be made warm for remote control.
		servicer.set_temperature(slots::VIRTUAL_BUTTONMAP, bheap::Block::TemperatureWarm);
	}
}

void ui::Buttons::update() {
	if (!last_update) last_update = xTaskGetTickCount();
	auto now = xTaskGetTickCount();
	TickType_t duration = now - last_update;
	if (duration) {
		last_update = now;
		last_duration = duration;
	}
	else last_duration = 0;

	last_held = current_held;
	current_held = GPIOA->IDR;

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

TickType_t ui::Buttons::frame_time() const {
	return last_duration;
}
