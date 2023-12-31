#ifndef MSN_UIB_H
#define MSN_UIB_H

#include <stdint.h>
#include <FreeRTOS.h>

#include "common/slots.h"

namespace ui {
	extern struct Buttons {
		// setup gpios
		void init();
		// Load new values from esp/buttons 
		void update();
		
		// Buttons are either physical, which include:
		// 	- MENU, TAB, SEL, POWER, and STICK
		// or are simulated from the joystick, for
		//  - UP, LEFT, DOWN and RIGHT
		enum Button {
			MENU = 1,
			TAB = 2,
			SEL = 4,
			POWER = 8,
			STICK = 16,

			UP = 32,
			LEFT = 64,
			DOWN = 128,
			RIGHT = 256,

			NXT = UP | RIGHT | TAB,
			PRV = DOWN | LEFT
		};

		enum Axis {
			X,
			Y
		};

		enum AdcState {
			ADC_UNINIT,
			ADC_UNCALIBRATED,
			ADC_DISABLED,
			ADC_READY
		};

		bool held(Button b, TickType_t minduration=0) const;
		bool press(Button b) const;
		bool rel(Button b) const;

		bool operator[](Button b) const {return press(b);}
		int  operator[](Axis b) const {
			if (!adc_ready())
				return 0;
			return b == X ? x_axis : y_axis;
		}
		bool changed() const;

		void read_raw_adc(uint16_t &x, uint16_t &y) const;
		AdcState adc_state() const { return cur_adc_state; }
		bool adc_ready() const { return cur_adc_state == ADC_READY; }

		void reload_adc_calibration() { cur_adc_state = ADC_UNINIT; }

		TickType_t frame_time() const;

		static int8_t adjust_adc_value(uint16_t raw, const slots::protocol::AdcCalibration::AxisCalibration& axis);
	private:
		uint16_t current_held{};
		uint16_t last_held{};
		uint16_t held_duration[9]{};

		int8_t x_axis{}, y_axis{};

		TickType_t last_update{};
		TickType_t last_duration{};

		slots::protocol::AdcCalibration adc_calibration{};
		AdcState cur_adc_state = ADC_UNINIT;

		mutable volatile bool adc_lock = false;
	} buttons;
}

#endif
