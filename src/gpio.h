#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "stm32f2xx.h"
#include "stm32f2xx_ll_gpio.h"

namespace gpio {
	template<uint32_t Port, uint32_t Mask>
	struct Pin {
		inline Pin& operator=(const bool& on) {
			if (on) {
				((GPIO_TypeDef *)Port)->BSRR |= (1 << Mask);
			}
			else {
				((GPIO_TypeDef *)Port)->BSRR |= (1 << (Mask+16));
			}
			return *this;
		}
	};
}

#endif 
