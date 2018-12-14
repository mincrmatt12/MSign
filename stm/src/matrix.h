#ifndef MATRIX_H
#define MATRIX_H

#include "gpio.h"
#include <stdint.h>
#include <string.h>
#include "stm32f2xx.h"
#include "stm32f2xx_ll_tim.h"
#include "stm32f2xx_ll_bus.h"
#include "stm32f2xx_ll_gpio.h"
#include "stm32f2xx_ll_dma.h"

namespace led {

	template<uint16_t Width, uint16_t Height>
		struct FrameBuffer {
			FrameBuffer() {
				memset(data, 0x0, sizeof(data));
			}

			void prepare_stream(uint16_t i, uint8_t pos) {
				uint8_t mask = (1 << pos);
				for (uint16_t j = 0, x = 0; j < (Width*2); j += 2, ++x) {
					byte_stream[j] = 
						(((_r(x, i)           & mask) == mask) ? 1  : 0) +
						(((_g(x, i)           & mask) == mask) ? 2  : 0) +
						(((_b(x, i)           & mask) == mask) ? 4  : 0) +
						(((_r(x, i+stb_lines) & mask) == mask) ? 8  : 0) +
						(((_g(x, i+stb_lines) & mask) == mask) ? 16 : 0) +
						(((_b(x, i+stb_lines) & mask) == mask) ? 32 : 0);
					byte_stream[j+1] = byte_stream[j] + 64;
				}
				byte_stream[(Width*2)] = 0x00;
			}

			uint8_t & r(uint16_t x, uint16_t y) {
				if (x < Width && y < Height)
					return _r(x, y);
				return junk;
			}
			uint8_t & g(uint16_t x, uint16_t y) {
				if (x < Width && y < Height)
					return _g(x, y);
				return junk;
			}
			uint8_t & b(uint16_t x, uint16_t y) {
				if (x < Width && y < Height)
					return _b(x, y);
				return junk;
			}

			uint8_t * byte_stream;

			static constexpr uint16_t width = Width;
			static constexpr uint16_t height = Height;
			static constexpr uint16_t stb_lines = Height / 2;

		private:
			uint8_t data[Width*Height*3];

			inline uint8_t & _r(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 0];
			}

			inline uint8_t & _g(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 1];
			}

			inline uint8_t & _b(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 2];
			}

			uint8_t junk; // used as failsafe for read/write out of bounds
		};

	template<typename FB>
		struct Matrix {
			Matrix() : fb0(), fb1() {
				fb0.byte_stream = this->dma_buffer;
				fb1.byte_stream = this->dma_buffer;
			}
			void init() {
				// Enable clocks
				LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1 | LL_APB2_GRP1_PERIPH_TIM9);	 // timer
				LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD | LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_DMA2 | LL_AHB1_GRP1_PERIPH_GPIOE); // gpios

				// Setup the timer.
				LL_TIM_InitTypeDef tim_init = {0};

				tim_init.Prescaler  = 5; // Set the timer to run at around 6Mhz, since it's stupid to do it any faster
				tim_init.Autoreload = 1;
				tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV2;
				LL_TIM_Init(TIM1, &tim_init);

				tim_init.Prescaler  = 36;
				tim_init.Autoreload = 1;
				tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV4;
				LL_TIM_Init(TIM9, &tim_init);

				LL_TIM_DisableARRPreload(TIM9);
				LL_TIM_DisableDMAReq_UPDATE(TIM1);
				LL_TIM_EnableIT_UPDATE(TIM1);

				// setup gpios
				LL_GPIO_InitTypeDef gpio_init = {0};
				gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
				gpio_init.Mode = LL_GPIO_MODE_OUTPUT;
				gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
				gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
				gpio_init.Pull = LL_GPIO_PULL_NO;
				LL_GPIO_Init(GPIOD, &gpio_init);
				gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6;
				LL_GPIO_Init(GPIOB, &gpio_init);
			}

			void display() {
				if (is_active()) return;
				blasting = true;
				// Set config values back to sane defaults
				pos = 0;
				row = 0;
				show = true;
				delaying = false;
				delay_counter = 0;
				// blank the thing
				oe = true; // active high
				// start the whole procedure
				blast_row();
			}

			void swap_buffers() {
				if (is_active()) return;
				active_buffer = !active_buffer;
			}

			FB& get_inactive_buffer() {return active_buffer ? fb1 : fb0;}
			const FB& get_active_buffer() {return active_buffer ? fb0 : fb1;}
			volatile bool is_active() {return this->blasting;}

			void dma_finish() {
				// blast is finished, stop dma + timer output
				LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
				LL_TIM_DisableDMAReq_UPDATE(TIM1);
				// check show
				if (show) do_next();
				else show = true;
			}

			void tim_elapsed() {
				LL_TIM_DisableCounter(TIM9);
				LL_TIM_DisableIT_UPDATE(TIM9);
				if (!delaying) return;
				oe = true;
				delaying = false;
				GPIOB->ODR = (GPIOB->ODR & 0xfff0) | (row & 0x000f);
				if (show) do_next();
				else show = true;
			}

		private:
			FB fb0, fb1;
			bool active_buffer = false;
			volatile bool blasting = false;
			uint8_t pos = 0;
			uint16_t row = 0;
			uint8_t dma_buffer[(FB::width * 2) + 1];

			// impl for the wait system
			uint16_t delay_counter = 0;
			bool delaying = false;

			// show variable
			bool show = false;

			// pins
			gpio::Pin<GPIOB_BASE, 5> strobe;
			gpio::Pin<GPIOB_BASE, 6> oe;
			
			// clock is tied to tim2_ch3

			FB& _get_active_buffer() {return active_buffer ? fb0 : fb1;}

			void blast_row() {
				// Setup le dma
				LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_5, LL_DMA_CHANNEL_6);
				LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_5, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
				LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_5, LL_DMA_PRIORITY_VERYHIGH);
				LL_DMA_SetMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MODE_NORMAL);
				LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_PERIPH_NOINCREMENT);
				LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MEMORY_INCREMENT);
				LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_5, LL_DMA_PDATAALIGN_BYTE);
				LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_5, LL_DMA_MDATAALIGN_BYTE);

				LL_DMA_DisableFifoMode(DMA2, LL_DMA_STREAM_5);
				LL_DMA_ConfigAddresses(DMA2, LL_DMA_STREAM_5, (uint32_t)dma_buffer, (uint32_t)(&GPIOD->ODR), LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

				LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, (FB::width * 2) + 1);

				// make sure the active buffer is ready
				_get_active_buffer().prepare_stream(row, pos);

				LL_DMA_DisableIT_HT(DMA2, LL_DMA_STREAM_5);
				LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_5);
				LL_DMA_EnableIT_TE(DMA2, LL_DMA_STREAM_5);
				LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_5);

				// blasting is a-go
				LL_TIM_EnableDMAReq_UPDATE(TIM1);
				LL_TIM_SetCounter(TIM1, 0);
				LL_TIM_EnableCounter(TIM1);
			}
			void wait(uint16_t ticks) {
				if (delaying) while (1) {;}
				// set the counter
				delaying = true;
				delay_counter = ticks;
				// enable the ticker (happens every 3 ticks of tim1, compute based on width * 3 * amt)
				LL_TIM_SetAutoReload(TIM9, ticks);
				LL_TIM_SetCounter(TIM9, 0);
				LL_TIM_EnableCounter(TIM9);
				LL_TIM_EnableIT_UPDATE(TIM9);
			}

			void do_next() {
				// set address pins
				if (row == (FB::stb_lines)) {
					// We are now finished
					blasting = false;
					// Stop the timer entirely
					LL_TIM_DisableCounter(TIM1);
					return;
				}
				strobe = true;
				show = false;
				strobe = false;
				uint8_t drawn_pos = pos;
				++pos;
				if (pos < 8) {
					// Start the waiting.
					oe = false;
					wait(1 << drawn_pos);		
					// Send off the next row
					blast_row();
					return;
				}
				else if (++row < FB::stb_lines) {
					oe = false;
					wait(1 << drawn_pos);		
					// Set back to first bit
					pos = 0;
					// Blast and wait
					blast_row();
					return;
				}
				else {
					// Alright, row is now == fb::height, so run the wait code
					// Just wait, but force it to run us again.
					show = true;
					oe = false;
					wait(1 << drawn_pos);
				}
			}
		};

}

#endif
