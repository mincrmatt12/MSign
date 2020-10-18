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
#include <FreeRTOS.h>
#include <task.h>

namespace led {
	template<uint16_t Width>
	struct BasicFrameBufferStorage {
		static const uint_fast16_t EffectiveWidth = Width;

		inline static uint16_t & _r(uint16_t * data, uint16_t x, uint16_t y) {
			return data[x*3 + y*Width*3 + 0];
		}

		inline static uint16_t & _g(uint16_t * data, uint16_t x, uint16_t y) {
			return data[x*3 + y*Width*3 + 1];
		}

		inline static uint16_t & _b(uint16_t * data, uint16_t x, uint16_t y) {
			return data[x*3 + y*Width*3 + 2];
		}
	};

	template<uint16_t Width, uint16_t Height, typename Storage=BasicFrameBufferStorage<Width>>
	struct FrameBuffer {
		FrameBuffer() {
			memset(data, 0x0, sizeof(data));
		}

		void clear() {
			memset(data, 0x0, sizeof(data));
		}

		void prepare_stream(uint16_t i, uint8_t pos) {
			// ARM assembler is fairly interesting 
			// we can have nearly any instruction execute conditionally
			// and the and operation will set flags if it ends with an S...

			uint8_t * lo = (uint8_t *)(((uintptr_t)(&data[i*Storage::EffectiveWidth*3]) & 0xFFFFF) * 32 + 0x22000000 + pos*4);
			uint8_t * hi = lo + stb_lines*Storage::EffectiveWidth*3*64;
			uint8_t * bs = &byte_stream[0];

			for (uint_fast16_t j = 0; j < Storage::EffectiveWidth; ++j) {
				asm volatile (
					// start by loading the bit value at lo into r2
					"ldrb r2, [%[Lo]], #64\n\t"  // increment by 64 each time to advance a word
					// load the next byte
					"ldrb r10, [%[Lo]], #64\n\t"
					// or them together
					"orr r2, r2, r10, lsl #1\n\t"
					// repeat for B byte
					"ldrb r10, [%[Lo]], #64\n\t"
					"orr r2, r2, r10, lsl #2\n\t" // now shift by 2 since it's in bit position 2
					// do the entire thing but in r3 to save an or
					"ldrb r3, [%[Hi]], #64\n\t"
					"ldrb r10, [%[Hi]], #64\n\t"
					"orr r3, r3, r10, lsl #1\n\t"
					"ldrb r10, [%[Hi]], #64\n\t"
					"orr r3, r3, r10, lsl #2\n\t"
					// move r3 into r2
					"orr r2, r2, r3, lsl #3\n\t"
					// now, r2 contains the value for bs
					"strb r2, [%[Bs]], #1\n\t" // store r2 into BS then add 1 to BS
					// set the clock line
					"orr r2, r2, #64\n\t"
					// store the byte
					"strb r2, [%[Bs]], #1\n\t" // store r2 into BS then add 1 to BS
					: [Lo]"=r"(lo), [Hi]"=r"(hi), [Bs]"=r"(bs)
					: "0"(lo), "1"(hi), "2"(bs)
					: "r2", "r3", "cc", "r10"
				);
			}
		}

		inline uint16_t & r(uint16_t x, uint16_t y) {
			if (on_screen(x, y)) {
				return Storage::_r(data, x, y);
			}
			return junk;
		}
		inline uint16_t & g(uint16_t x, uint16_t y) {
			if (on_screen(x, y)) {
				return Storage::_g(data, x, y);
			}
			return junk;
		}
		inline uint16_t & b(uint16_t x, uint16_t y) {
			if (on_screen(x, y)) {
				return Storage::_b(data, x, y);
			}
			return junk;
		}

		inline const uint16_t & r(uint16_t x, uint16_t y) const {
			if (on_screen(x, y)) {
				return Storage::_r(data, x, y);
			}
			return junk;
		}
		inline const uint16_t & g(uint16_t x, uint16_t y) const {
			if (on_screen(x, y)) {
				return Storage::_g(data, x, y);
			}
			return junk;
		}
		inline const uint16_t & b(uint16_t x, uint16_t y) const {
			if (on_screen(x, y)) {
				return Storage::_b(data, x, y);
			}
			return junk;
		}

		inline bool on_screen(uint16_t x, uint16_t y) const {
			return (x < Width && y < Height);
		}

		uint8_t * byte_stream;

		static constexpr uint16_t width = Width;
		static constexpr uint16_t effective_width = Storage::EffectiveWidth;
		static constexpr uint16_t height = Height;
		static constexpr uint16_t stb_lines = 16;

	protected:
		uint16_t data[Width*Height*3];

		uint16_t junk; // used as failsafe for read/write out of bounds
	};

	struct FourPanelSnake {
		//6<------------------------
		//5<------------------------
		//4<------------------------
		//1------------------------>
		//2------------------------>
		//3------------------------>
		
		const static uint_fast16_t EffectiveWidth = 256;


		inline static uint16_t & _r(uint16_t * data, uint16_t x, uint16_t y) {
			if (y > 31) 
				return data[x*3 + (y & 0x1f)*256*3];
			else
				return data[(255 - x)*3 + (31 - y)*256*3];
		}

		inline static uint16_t & _g(uint16_t * data, uint16_t x, uint16_t y) {
			if (y > 31) 
				return data[x*3 + (y & 0x1f)*256*3 + 1];
			else
				return data[(255 - x)*3 + (31 - y)*256*3 + 1];
		}

		inline static uint16_t & _b(uint16_t * data, uint16_t x, uint16_t y) {
			if (y > 31) 
				return data[x*3 + (y & 0x1f)*256*3 + 2];
			else
				return data[(255 - x)*3 + (31 - y)*256*3 + 2];
		}
	};

	template<typename FB>
	struct Matrix {
		typedef FB framebuffer_type;

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
			LL_GPIO_InitTypeDef gpio_init = {0};
			gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
			gpio_init.Mode = LL_GPIO_MODE_OUTPUT;
			gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
			gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
			gpio_init.Pull = LL_GPIO_PULL_DOWN;
			LL_GPIO_Init(SIGN_DATA_Port, &gpio_init);
			gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6;
			gpio_init.Pull = LL_GPIO_PULL_NO;
			LL_GPIO_Init(GPIOB, &gpio_init);
		}

		void start_display() {
			// Set config values back to sane defaults
			pos = 0;
			row = 0;
			show = true;
			delaying = false;
			should_swap = false;
			delay_counter = 0;
			// blank the thing
			oe = true; // active high
			// start the whole procedure
			blast_row();
		}

		// This should only be called from an RTOS task
		void swap_buffers() {
			//active_buffer = !active_buffer;
			notify_when_swapped = xTaskGetCurrentTaskHandle();
			should_swap = true;

			// This uses the task notification value to wait for something to happen.
			// This _does_ preclude the use of notification values for anything else.
			xTaskNotifyWait(0, 0xffffffffUL, NULL, portMAX_DELAY);
		}

		void swap_buffers_from_isr() {
			notify_when_swapped = nullptr;
			should_swap = true;
			while (should_swap) {;}
		}

		FB& get_inactive_buffer() {return active_buffer ? fb1 : fb0;}
		const FB& get_active_buffer() {return active_buffer ? fb0 : fb1;}

		void dma_finish() {
			// blast is finished, stop dma + timer output
			LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
			LL_TIM_DisableCounter(TIM1);
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
			if (show) do_next();
			else {
				show = true;
			}
		}

	private:
		FB fb0, fb1;
		bool active_buffer = false;
		uint8_t pos = 0;
		uint16_t row = 0;
		uint8_t dma_buffer[(FB::effective_width * 2) + 3];
		TaskHandle_t notify_when_swapped = nullptr;

		// impl for the wait system
		uint16_t delay_counter = 0;
		bool delaying = false;

		// show variable
		bool show = false;

		// should swap buffers
		bool should_swap = false;

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

			LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, (FB::effective_width * 2) + 1);

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
			delay_counter = (ticks + 2);
			// enable the ticker (happens every 3 ticks of tim1, compute based on width * 3 * amt)
			LL_TIM_SetAutoReload(TIM9, ticks);
			LL_TIM_SetCounter(TIM9, 0);
			LL_TIM_EnableCounter(TIM9);
			LL_TIM_EnableIT_UPDATE(TIM9);
		}

		void do_next() {
			// set address pins
			if (pos == 12) {
				// We have finished clocking out a row, process buffer swaps
				if (should_swap) {
					active_buffer = !active_buffer;
					if (notify_when_swapped) xTaskNotifyFromISR(notify_when_swapped, 1, eSetValueWithOverwrite, NULL);
				}
				// Start the display again
				start_display();
				return;
			}
			oe = true;
			strobe = true;
			show = false;
			// Thing
			GPIOB->ODR = (GPIOB->ODR & 0xfff0) | (row & 0x000f);
			const static uint16_t draw_ticks_table[] = {3,6,12,24,48,96,192,384,768,1536,3072,6144};
			uint16_t drawn_pos = draw_ticks_table[pos];
			++row;
			if (row < FB::stb_lines) {
				strobe = false;
				// Send off the next row
				blast_row();
				// Start the waiting.
				oe = false;
				wait(drawn_pos);		
				return;
			}
			else if (++pos < 12) {
				strobe = false;
				// Set back to first row
				row = 0;
				// Blast and wait
				blast_row();
				oe = false;
				wait(drawn_pos);		
				return;
			}
			else {
				strobe = false;
				// Alright, pos == 12, so run the wait code
				// Just wait, but force it to run us again.
				show = true;
				oe = false;
				wait(drawn_pos);
			}
		}
	};

}

#endif
