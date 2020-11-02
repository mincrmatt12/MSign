#ifndef SIMPLMATRIX_H
#define SIMPLMATRIX_H

#include "../gpio.h"
#include <stdint.h>
#include <string.h>
#include "stm32f2xx.h"
#include "stm32f2xx_ll_tim.h"
#include "stm32f2xx_ll_bus.h"
#include "stm32f2xx_ll_gpio.h"
#include "stm32f2xx_ll_dma.h"
#include "../pins.h"

namespace crash {
	// Minimal version of the real matrix code, only supporting 2-bit color and single-buffered.
	struct Matrix {
		Matrix() {}

		void init() {
			// Enable clocks
			LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1 | LL_APB2_GRP1_PERIPH_TIM9);	 // timer
			LL_AHB1_GRP1_EnableClock(SIGN_GPIO_PERIPH | LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_DMA2); // gpios

			// Reset GPIO / DMA / TIMER
			LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_TIM1 | LL_APB2_GRP1_PERIPH_TIM9);
			LL_AHB1_GRP1_ForceReset(LL_AHB1_GRP1_PERIPH_DMA2); // gpios
			asm volatile ("nop");
			asm volatile ("nop");
			asm volatile ("nop");
			LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_TIM1 | LL_APB2_GRP1_PERIPH_TIM9);
			LL_AHB1_GRP1_ReleaseReset(LL_AHB1_GRP1_PERIPH_DMA2); // gpios

			LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1 | LL_APB2_GRP1_PERIPH_TIM9);	 // timer
			LL_AHB1_GRP1_EnableClock(SIGN_GPIO_PERIPH | LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_DMA2); // gpios

			// Setup the timer.
			LL_TIM_InitTypeDef tim_init{};

			tim_init.Prescaler  = 3; // Set the timer to run at around 6Mhz, since it's stupid to do it any faster
			tim_init.Autoreload = 1;
			tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
			LL_TIM_Init(TIM1, &tim_init);

			tim_init.Prescaler  = 2;
			tim_init.Autoreload = 1;
			tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
			LL_TIM_Init(TIM9, &tim_init);

			LL_TIM_DisableARRPreload(TIM9);
			LL_TIM_DisableDMAReq_UPDATE(TIM1);
			LL_TIM_EnableIT_UPDATE(TIM1);

			// setup gpios
			LL_GPIO_InitTypeDef gpio_init{};
			gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
			gpio_init.Mode = LL_GPIO_MODE_OUTPUT;
			gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
			gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
			gpio_init.Pull = LL_GPIO_PULL_DOWN;
			LL_GPIO_Init(SIGN_DATA_Port, &gpio_init);
			gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6;
			gpio_init.Pull = LL_GPIO_PULL_NO;
			LL_GPIO_Init(GPIOB, &gpio_init);
		}

		uint8_t frame[256*32];

		uint8_t & color(uint16_t x, uint16_t y) {
			if (y > 31) {
				return frame[x + (y & 0x1f)*256];
			}
			else {
				return frame[(255 - x) + (31 - y)*256];
			}
		}
	};

}

#endif
