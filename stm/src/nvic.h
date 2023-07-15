#ifndef NVIC_H
#define NVIC_H

namespace nvic {
	const inline int MSignSWTrap_IRQn = 61;

	void init();
	void setup_isrs_for_flash(bool enable=true);
}

#endif
