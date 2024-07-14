#include "ui.h"

#include "pins.h"
#include "srv.h"
#include <numeric>
#include <algorithm>

#ifndef SIM

#ifdef USE_F2
#include <stm32f2xx_ll_bus.h>
#include <stm32f2xx_ll_gpio.h>
#endif
#ifdef USE_F4
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>
#endif

#endif

extern srv::Servicer servicer;

ui::Buttons ui::buttons;

namespace ui {
	const uint32_t physical_button_masks[]{
		/* [Buttons::MENU] = */ (1 << 7),
		/* [Buttons::TAB] = */ (1 << 9),
		/* [Buttons::SEL] = */ (1 << 8),
		/* [Buttons::POWER] = */ (1 << 0),
		/* [Buttons::STICK] = */ (1 << 10)
	};

	const inline int adc_x_pin = 5,
		             adc_y_pin = 6;
}

#ifndef SIM

void ui::Buttons::init() {
	LL_AHB1_GRP1_EnableClock(UI_BUTTON_PERIPH);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);

	LL_GPIO_InitTypeDef it{};

	it.Mode = LL_GPIO_MODE_INPUT;
	it.Pull = LL_GPIO_PULL_NO;
	it.Speed = LL_GPIO_SPEED_FREQ_LOW;
	it.Pin = 0;
	for (auto pinmask : physical_button_masks)
		it.Pin |= pinmask;

	LL_GPIO_Init(UI_BUTTON_PORT, &it);
	it.Mode = LL_GPIO_MODE_ANALOG;
	it.Pin = (1 << adc_x_pin) | (1 << adc_y_pin);
	LL_GPIO_Init(UI_BUTTON_PORT, &it);

	// Enable the ADC & do single conversions
	ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EOCS;

	// Sample for 84 cycles to try and cancel out even more noise
	ADC1->SMPR2 = ADC_SMPR2_SMP6_2 | ADC_SMPR2_SMP5_2;

	// Sample only a single channel at a time.
	ADC1->SQR1 = 0;
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

	{
		current_held = 0;
		uint16_t ormask = 1;
		uint32_t sampled = UI_BUTTON_PORT->IDR;
		for (auto pinmask : physical_button_masks) {
			if (sampled & pinmask)
				current_held |= ormask;
			ormask <<= 1;
		}
	}

	if (adc_ready()) {
		uint16_t raw_x, raw_y;
		read_raw_adc(raw_x, raw_y);

		x_axis = adjust_adc_value(raw_x, adc_calibration.x);
		y_axis = adjust_adc_value(raw_y, adc_calibration.y);

		if (x_axis > 100) current_held |= RIGHT;
		else if (x_axis < -100) current_held |= LEFT;
		if (y_axis < -100) current_held |= UP;
		else if (y_axis > 100) current_held |= DOWN;
	}
	else if (cur_adc_state == ADC_UNINIT && servicer.ready()) {
		switch (servicer.get_adc_calibration(adc_calibration)) {
			case slots::protocol::AdcCalibrationResult::OK:
				cur_adc_state = ADC_READY;
				break;
			case slots::protocol::AdcCalibrationResult::MISSING:
				cur_adc_state = ADC_UNCALIBRATED;
				break;
			case slots::protocol::AdcCalibrationResult::MISSING_IGNORE:
				cur_adc_state = ADC_DISABLED;
				break;
			case slots::protocol::AdcCalibrationResult::TIMEOUT:
				break;
		}
	}

	for (int i = 0; i < 9; ++i) {
		if (current_held & (1 << i)) {
			held_duration[i] += duration;
		}
		else held_duration[i] = 0;
	}
}

#endif

int8_t ui::Buttons::adjust_adc_value(uint16_t raw, const slots::protocol::AdcCalibration::AxisCalibration& axis) {
	if (axis.min > axis.max) {
		slots::protocol::AdcCalibration::AxisCalibration fixed {
			.min = axis.max,
			.max = axis.min,
			.deadzone = axis.deadzone
		};
		return -adjust_adc_value(raw, fixed);
	}
	raw = std::min<uint16_t>(std::max<uint16_t>(axis.min, raw), axis.max);
	uint16_t deadzone_mid = std::midpoint(axis.min, axis.max);
	if (raw > deadzone_mid && raw - deadzone_mid < axis.deadzone)
		return 0;
	else if (raw < deadzone_mid && deadzone_mid - raw < axis.deadzone)
		return 0;
	else {
		return ((int)raw - (int)deadzone_mid) * 127 / (deadzone_mid - axis.min);
	}
}

bool ui::Buttons::held(Button b, TickType_t mindur) const {
	uint16_t mask = b;
	if (mask & (mask - 1)) {
		if (!(current_held & mask)) return false;
		for (auto check : held_duration) {
			if (!mask) break;
			if (mask & 1 && check >= mindur) return true;
			mask >>= 1;
		}
		return false;
	}
	else {
		return current_held & mask && held_duration[__builtin_ctz(b)] >= mindur;
	}
}

bool ui::Buttons::rel(Button b) const {
	return !held(b) && (current_held ^ last_held) & b;
}

bool ui::Buttons::press(Button b) const {
	return held(b) && (current_held ^ last_held) & b;
}

bool ui::Buttons::changed() const {
	return current_held ^ last_held;
}

int ui::Buttons::frame_time() const {
	return last_duration;
}

#ifndef SIM

void ui::Buttons::read_raw_adc(uint16_t &x, uint16_t &y) const {
	if (adc_lock) {
		x = 0;
		y = 0;
		return;
	}
	adc_lock = true;
	ADC1->SR   = ~ADC_SR_EOC;
	ADC1->SQR3 = (adc_x_pin << ADC_SQR3_SQ1_Pos);
	ADC1->CR2 |= ADC_CR2_SWSTART;
	while (!(ADC1->SR & ADC_SR_EOC)) {;} // wait for x axis
	ADC1->SR   = ~ADC_SR_EOC;
	x = ADC1->DR;
	ADC1->SQR3 = (adc_y_pin << ADC_SQR3_SQ1_Pos);
	ADC1->CR2 |= ADC_CR2_SWSTART;
	while (!(ADC1->SR & ADC_SR_EOC)) {;} // wait for x axis
	ADC1->SR   = ~ADC_SR_EOC;
	y = ADC1->DR;
	adc_lock = false;
}

#endif
