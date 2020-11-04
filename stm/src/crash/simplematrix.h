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
	// Lookup table for color->bitplane conversion
	
	template<size_t Bit>
	struct MatrixBitLookupTable {
		uint8_t table[256]{};

		constexpr MatrixBitLookupTable() {
			for (int i = 0; i < 256; ++i) {
				bool r = i & (1 << Bit);
				bool g = i & (1 << (Bit + 2));
				bool b = i & (1 << (Bit + 4));

				table[i] = r | (g << 1) | (b << 2);
			}
		}
	};

	constexpr MatrixBitLookupTable<0> mblt0{};
	constexpr MatrixBitLookupTable<1> mblt1{};

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

			LL_TIM_DisableCounter(TIM9);
			LL_TIM_DisableCounter(TIM1);

			tim_init.Prescaler  = 8; // Set the timer to run at around 6Mhz, since it's stupid to do it any faster
			tim_init.Autoreload = 1;
			tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
			LL_TIM_Init(TIM1, &tim_init);

			tim_init.Prescaler  = 5;
			tim_init.Autoreload = 1;
			tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
			LL_TIM_Init(TIM9, &tim_init);

			LL_TIM_DisableARRPreload(TIM9);
			LL_TIM_EnableDMAReq_UPDATE(TIM1);
			LL_TIM_EnableIT_UPDATE(TIM9);

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

			memset(frame, 0, sizeof(frame));
			row = 0;
			bitplane = false;

			oe = true;
			strobe = false;
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

		void dma_finished() {
			// Stop DMA
			LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
			LL_TIM_DisableCounter(TIM1);

			// Set strobe 
			strobe = true;

			// Configure timer 9 (and give panels a bit to respond to the strobe signal)
			LL_TIM_SetAutoReload(TIM9, bitplane ? 1200 : 600);
			LL_TIM_SetCounter(TIM9, 0);
			
			// Finish strobe
			strobe = false;

			// Start counting
			LL_TIM_EnableCounter(TIM9);

			// Enable panels
			oe = false;
		}

		void timer_finished() {
			// Stop timer
			LL_TIM_DisableCounter(TIM9);
			// Stop output
			oe = true;

			// Can we send the next row?
			if (row == 15) {
				// No, send next bitplane
				bitplane = !bitplane;
				row = 0;
			}
			else {
				++row;
			}

			// Start DMA-ing the next row
			start_dmaing_row();

			// Setup the next row
			GPIOB->ODR = (GPIOB->ODR & 0xfff0) | (row & 0x000f);
		}

		void start_display() {
			// Set row to 0 (matches initial state)
			GPIOB->ODR = (GPIOB->ODR & 0xfff0);

			// Start dma
			start_dmaing_row();
		}

	private:
		uint8_t dma_buffer[513];
		uint8_t row;
		
		bool bitplane;

		// pins
		gpio::Pin<GPIOB_BASE, 5> strobe;
		gpio::Pin<GPIOB_BASE, 6> oe;

		// Start dma-ing a row.
		void start_dmaing_row() {
			// Set up the dma buffer
			for (int i = 0, j = 0; i < 256; ++i, j+=2) {
				dma_buffer[j] = (bitplane ? mblt1.table : mblt0.table)[frame[row*256 + i]] |
						 ((bitplane ? mblt1.table : mblt0.table)[frame[(row+16)*256 + i]]) << 3;
				dma_buffer[j+1] = dma_buffer[j] | 64; // set clock
			}

			dma_buffer[512] = 0;
			// Setup the DMA
			LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_5, LL_DMA_CHANNEL_6);
			LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_5, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
			LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_5, LL_DMA_PRIORITY_VERYHIGH);
			LL_DMA_SetMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MODE_NORMAL);
			LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_PERIPH_NOINCREMENT);
			LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MEMORY_INCREMENT);
			LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_5, LL_DMA_PDATAALIGN_BYTE);
			LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_5, LL_DMA_MDATAALIGN_BYTE);

			LL_DMA_DisableFifoMode(DMA2, LL_DMA_STREAM_5);
			LL_DMA_ConfigAddresses(DMA2, LL_DMA_STREAM_5, (uint32_t)dma_buffer, (uint32_t)(&SIGN_DATA_Port->ODR), LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

			LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, 513);

			LL_DMA_DisableIT_HT(DMA2, LL_DMA_STREAM_5);
			LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_5);
			LL_DMA_EnableIT_TE(DMA2, LL_DMA_STREAM_5);

			// Start sending bits
			LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_5);
			LL_TIM_EnableCounter(TIM1);
		}
	};

}

#endif
