#ifndef MSN_UIB_H
#define MSN_UIB_H

#include <stdint.h>
#include <FreeRTOS.h>

namespace ui {
	extern struct Buttons {
		// setup gpios
		void init();
		// Load new values from esp/buttons 
		void update();
		
		enum Button {
			POWER = 0, // PA0
			NXT = 10,
			PRV = 9,
			SEL = 8,
			MENU = 7,

			EXT1 = 6,
			EXT2 = 5
		};

		bool held(Button b, TickType_t minduration=0) const;
		bool press(Button b) const;
		bool rel(Button b) const;

		bool operator[](Button b) const {return press(b);}
		bool changed() const;
	private:
		uint16_t current_held{};
		uint16_t last_held{};
		uint16_t held_duration[11]{};
		TickType_t last_update{};
	} buttons;
}

#endif
