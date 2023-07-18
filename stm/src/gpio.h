#ifndef GPIO_H
#define GPIO_H

#ifdef USE_F2
#include <stm32f2xx.h>
#include <stm32f2xx_ll_gpio.h>
#endif

#ifdef USE_F4
#include <stm32f4xx.h>
#include <stm32f4xx_ll_gpio.h>
#endif

#include <stdint.h>

namespace gpio {
	template<uint32_t Port, uint32_t Mask>
	struct Pin {
		__attribute__((always_inline)) inline Pin& operator=(const bool& on) {
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
