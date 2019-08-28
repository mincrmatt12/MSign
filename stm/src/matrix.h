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
#include "pins.h"

namespace led {

	template<uint16_t Width, uint16_t Height>
		struct FrameBuffer {
			FrameBuffer() {
				memset(data, 0x0, sizeof(data));
			}

			void prepare_stream(uint16_t i, uint8_t pos) {
				// ARM assembler is fairly interesting 
				// we can have nearly any instruction execute conditionally
				// and the and operation will set flags if it ends with an S...

				uint8_t * lo = &_r(0, i);
				uint8_t * hi = &_r(0, i|stb_lines);
				uint8_t * bs = &byte_stream[0];
				uint_fast8_t mask = (1 << pos);

				for (uint_fast16_t j = 0; j < Width; ++j) {
					asm volatile (
						"ldrb r2, [%[Lo]], #1\n\t" // First, we load the value at _lo_ to r2, which we'll write at the end
						"tst r2, %[Mask]\n\t" // Do a clobber-less and
						"ite eq\n\t"  			// thumb requires explicit checks on the flags
						"moveq r2, #0\n\t"    // If the result is zero, set to 0
						"movne r2, #1\n\t"    // Otherwise, set to 1
						// different pattern for the rest of them
						"ldrb r10, [%[Lo]], #1\n\t" // Load the g value to r10
						"tst r10, %[Mask]\n\t"      // Again, set flags for r10 & mask
						"it ne\n\t"
						"orrne r2, r2, #2\n\t" // if result is nonzero or with 2
						// this continues for the b bit
						"ldrb r10, [%[Lo]], #1\n\t" // Load the g value to r10
						"tst r10, %[Mask]\n\t"      // Again, set flags for r10 & mask
						"it ne\n\t"
						"orrne r2, r2, #4\n\t" // if result is nonzero or with 4
						// now, we switch to the Hi byte
						"ldrb r10, [%[Hi]], #1\n\t" // Load the g value to r10
						"tst r10, %[Mask]\n\t"      // Again, set flags for r10 & mask
						"it ne\n\t"
						"orrne r2, r2, #8\n\t" // if result is nonzero or with 8
						// this continues for the g bit
						"ldrb r10, [%[Hi]], #1\n\t" // Load the g value to r10
						"tst r10, %[Mask]\n\t"      // Again, set flags for r10 & mask
						"it ne\n\t"
						"orrne r2, r2, #16\n\t" // if result is nonzero or with 16
						// this continues for the b bit
						"ldrb r10, [%[Hi]], #1\n\t" // Load the g value to r10
						"tst r10, %[Mask]\n\t"      // Again, set flags for r10 & mask
						"it ne\n\t"
						"orrne r2, r2, #32\n\t" // if result is nonzero or with 32
						// now, r2 contains the value for bs
						"strb r2, [%[Bs]], #1\n\t" // store r2 into BS then add 1 to BS
						// the next byte requires or 16
						"orr r2, r2, #64\n\t" // set the clock line
						// store the byte
						"strb r2, [%[Bs]], #1\n\t" // store r2 into BS then add 1 to BS
						: [Lo]"=r"(lo), [Hi]"=r"(hi), [Bs]"=r"(bs)
						: "0"(lo), "1"(hi), "2"(bs), [Mask]"r"(mask)
						: "r2", "cc", "r10"
					);
				}
			}

			uint8_t & r(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _r(x, y);
				return junk;
			}
			uint8_t & g(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _g(x, y);
				return junk;
			}
			uint8_t & b(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _b(x, y);
				return junk;
			}

			const uint8_t & r(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _r(x, y);
				return junk;
			}
			const uint8_t & g(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _g(x, y);
				return junk;
			}
			const uint8_t & b(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _b(x, y);
				return junk;
			}

			inline bool on_screen(uint16_t x, uint16_t y) const {
				return (x < Width && y < Height);
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
				LL_AHB1_GRP1_EnableClock(SIGN_GPIO_PERIPH | LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_DMA2); // gpios

				// Setup the timer.
				LL_TIM_InitTypeDef tim_init = {0};

				tim_init.Prescaler  = 7; // Set the timer to run at around 6Mhz, since it's stupid to do it any faster
				tim_init.Autoreload = 2;
				tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
				LL_TIM_Init(TIM1, &tim_init);

				tim_init.Prescaler  = 29;
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
				LL_GPIO_Init(SIGN_DATA_Port, &gpio_init);
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
				asm volatile ("nop");
				asm volatile ("nop");  // give the display some time to respond
				asm volatile ("nop");
				delaying = false;
				GPIOB->ODR = (GPIOB->ODR & 0xfff0) | (row & 0x000f);
				asm volatile ("nop");
				asm volatile ("nop");  // give the display some time to respond
				asm volatile ("nop");
				if (show) do_next();
				else show = true;
			}

		private:
			FB fb0, fb1;
			bool active_buffer = false;
			volatile bool blasting = false;
			uint8_t pos = 0;
			uint16_t row = 0;
			uint8_t dma_buffer[(FB::width * 2) + 3];

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
				LL_DMA_ConfigAddresses(DMA2, LL_DMA_STREAM_5, (uint32_t)dma_buffer, (uint32_t)(&SIGN_DATA_Port->ODR), LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

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
				delay_counter = (ticks + 2) << 1;
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
