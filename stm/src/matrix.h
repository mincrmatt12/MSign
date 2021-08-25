#ifndef MATRIX_H
#define MATRIX_H

#include "crash/main.h"
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
#include "color.h"

namespace led {
	template<uint16_t Width>
	struct BasicFrameBufferStorage {
		static const uint_fast16_t EffectiveWidth = Width;

		inline static color_t & _at(color_t * data, uint16_t x, uint16_t y) {
			return data[x + y*Width];
		}

		inline static const color_t & _at(const color_t * data, uint16_t x, uint16_t y) {
			return data[x + y*Width];
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
			switch (pos) {
				case 0:
					prepare_stream_bf<0>(i);
					return;
				case 1:
					prepare_stream_bf<1>(i);
					return;
				case 2:
					prepare_stream_bf<2>(i);
					return;
				case 3:
					prepare_stream_bf<3>(i);
					return;
				case 4:
					prepare_stream_bf<4>(i);
					return;
				case 5:
					prepare_stream_bf<5>(i);
					return;
				case 6:
					prepare_stream_bf<6>(i);
					return;
				case 7:
					prepare_stream_bf<7>(i);
					return;
				case 8:
					prepare_stream_bf<8>(i);
					return;
				case 9:
					prepare_stream_bf<9>(i);
					return;
				case 10:
					prepare_stream_bf<10>(i);
					return;
				case 11:
					prepare_stream_bf<11>(i);
					return;
				default:
					__builtin_unreachable();
			}
		}

		inline color_t & at(uint16_t x, uint16_t y) {
			if (on_screen(x, y)) {
				return Storage::_at(data, x, y);
			}
			return junk;
		}

		inline color_t & at_unsafe(uint16_t x, uint16_t y) {
			return Storage::_at(data, x, y);
		}

		inline const color_t & at(uint16_t x, uint16_t y) const {
			if (on_screen(x, y)) {
				return Storage::_at(data, x, y);
			}
			return junk;
		}

		inline const color_t & at_unsafe(uint16_t x, uint16_t y) const {
			return Storage::_at(data, x, y);
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
		color_t data[Width*Height];

		color_t junk; // used as failsafe for read/write out of bounds

	private:
		template<unsigned Pos>
		void prepare_stream_bf(uint16_t i) {
			uint16_t * lo = &data[i*Storage::EffectiveWidth].r;
			uint16_t * hi = lo + stb_lines*Storage::EffectiveWidth*3;
			uint8_t  * bs = byte_stream;

			for (uint_fast16_t j = 0; j < Storage::EffectiveWidth / 2; ++j) {
				asm volatile (
					// Load 3 words (or 6 colors / 2 pixels) into r1-r3 using ldm
					"ldmia %[Lo]!, {r1,r2,r3}\n\t"
					// Grab green0
					"ubfx r10, r1, %[PosPlus], #1\n\t"
					// Place red0  (r1 is now    g16r
					"ubfx r1, r1, %[Pos], #1\n\t"
					// Place red1
					"lsr r2, r2, %[Pos]\n\t"
					// Grab blue0
					"bfi r10, r2, #1, #1\n\t"
					// Mask red0
					//"ubfx r1, r1, #1, #1\n\t"
					// Or into r1
					"bfi r1, r10, #1, #2\n\t" /// now r1 contains <junk>bgr in correct order
					// Extract green1 into r10
					"ubfx r10, r3, %[Pos], #1\n\t"
					// Prepare pixel2
					"lsr r3, r3, %[PosPlus]\n\t"
					// Insert
					"bfi r10, r3, #1, #1\n\t"
					// Bfi again to create final value to be orred
					"bfi r2, r10, #17, #2\n\t" /// now r2 contains <junk>bgr<junk>
					// Mask out r2
					"and r2, r2, #0x00070000\n\t"
					// Prepare bit sequence
					"orr r11, r1, r2\n\t"
					// Load bottom 3 bytes
				    
					// Load 3 words (or 6 colors / 2 pixels) into r1-r3 using ldm
					"ldmia %[Hi]!, {r1,r2,r3}\n\t"
					// Grab green0
					"ubfx r10, r1, %[PosPlus], #1\n\t"
					// Place red0  (r1 is now    g16r
					"ubfx r1, r1, %[Pos], #1\n\t"
					// Place red1
					"lsr r2, r2, %[Pos]\n\t"
					// Grab blue0
					"bfi r10, r2, #1, #1\n\t"
					// Mask red0
					//"ubfx r1, r1, #1, #1\n\t"
					// Or into r1
					"bfi r1, r10, #1, #2\n\t" /// now r1 contains <junk>bgr in correct order
					// Extract green1 into r10
					"ubfx r10, r3, %[Pos], #1\n\t"
					// Prepare pixel2
					"lsr r3, r3, %[PosPlus]\n\t"
					// Insert
					"bfi r10, r3, #1, #1\n\t"
					// Bfi again to create final value to be orred
					"bfi r2, r10, #17, #2\n\t" /// now r2 contains <junk>bgr<junk>
					"and r2, r2, #0x00070000\n\t"
					"orr r10, r1, r2\n\t"
					// Prepare actual value
					"orr r11, r11, r10, lsl #3\n\t"
					"orr r11, r11, r11, lsl #8\n\t"
					"orr r11, r11, #0x40004000\n\t"
					// Store value 
					"str r11, [%[Bs]], #4\n\t"
					: [Lo]"=r"(lo), [Hi]"=r"(hi), [Bs]"=r"(bs) 
					: "0"(lo), "1"(hi), "2"(bs), [Pos]"i"(Pos), [PosPlus]"i"(Pos + 16)
					: "r1", "r2", "r3", "cc", "r10", "r11"
				);
			}
		}
	};

	struct FourPanelSnake {
		//6<------------------------
		//5<------------------------
		//4<------------------------
		//1------------------------>
		//2------------------------>
		//3------------------------>
		
		const static uint_fast16_t EffectiveWidth = 256;

		inline static color_t & _at(color_t * data, uint_fast16_t x, uint_fast16_t y) {
			x = 127 - x;
			y = 63 - y;

			return data[(x + (y / 32)*128) + (y & 31)*EffectiveWidth];
		}

		inline static const color_t & _at(const color_t * data, uint_fast16_t x, uint_fast16_t y) {
			x = 127 - x;
			y = 63 - y;

			return data[(x + (y / 32)*128) + (y & 31)*EffectiveWidth];
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
			LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);	 // timer
			LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);
			LL_AHB1_GRP1_EnableClock(SIGN_GPIO_PERIPH | LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_DMA2); // gpios

			// Setup the timer.
			LL_TIM_InitTypeDef tim_init = {0};

			tim_init.Prescaler  = 3; // Set the timer to run at around 6Mhz, since it's stupid to do it any faster
			tim_init.Autoreload = 1;
			tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
			LL_TIM_Init(TIM1, &tim_init);

			tim_init.Prescaler  = 2;
			tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
			LL_TIM_Init(TIM4, &tim_init);

			LL_TIM_EnableDMAReq_UPDATE(TIM1); // trigger dma requests every cycle of tim1

			// setup gpios
			LL_GPIO_InitTypeDef gpio_init = {0};
			gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
			gpio_init.Mode = LL_GPIO_MODE_OUTPUT;
			gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
			gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
			gpio_init.Pull = LL_GPIO_PULL_DOWN;
			LL_GPIO_Init(SIGN_DATA_Port, &gpio_init);
			gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5;
			gpio_init.Pull = LL_GPIO_PULL_NO;
			LL_GPIO_Init(GPIOB, &gpio_init);
			gpio_init.Pin = LL_GPIO_PIN_6;
			gpio_init.Alternate = LL_GPIO_AF_2;
			gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
			LL_GPIO_Init(GPIOB, &gpio_init);

			// enable interrupt on oe wait finish
			LL_TIM_EnableIT_UPDATE(TIM4);

			LL_TIM_OC_InitTypeDef tim_oc_init = {0};
			tim_oc_init.CompareValue = 1; // compare to 1
			tim_oc_init.OCMode = LL_TIM_OCMODE_PWM2; // output active while value is not less than 1
			tim_oc_init.OCPolarity = LL_TIM_OCPOLARITY_LOW; // output is active low
			tim_oc_init.OCState = LL_TIM_OCSTATE_ENABLE; // channel is enabled
			LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH1, &tim_oc_init);

			// set one pulse mode
			LL_TIM_SetOnePulseMode(TIM4, LL_TIM_ONEPULSEMODE_SINGLE);

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

			LL_DMA_DisableIT_HT(DMA2, LL_DMA_STREAM_5);
			LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_5);
			LL_DMA_EnableIT_TE(DMA2, LL_DMA_STREAM_5);
		}

		void start_display() {
			// Set config values back to sane defaults
			pos = 0;
			row = 0;
			show = true;
			delaying = false;
			should_swap = false;
			delay_counter = 0;
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
			LL_TIM_DisableCounter(TIM1);
			LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
			// check show
			if (show) do_next();
			else show = true;
		}

		void tim_elapsed() {
			if (!delaying) return;
			// we don't bother clearing counter enable since OPM will do it for us.
			delaying = false;
			if (show) do_next();
			else {
				show = true;
			}
		}

		int16_t frames_without_refresh = 0;

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
		
		// clock is tied to tim2_ch3

		FB& _get_active_buffer() {return active_buffer ? fb0 : fb1;}

		void blast_row() {
			LL_DMA_ConfigAddresses(DMA2, LL_DMA_STREAM_5, (uint32_t)dma_buffer, (uint32_t)(&SIGN_DATA_Port->ODR), LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
			LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, (FB::effective_width * 2) + 1);

			// make sure the active buffer is ready
			_get_active_buffer().prepare_stream(row, pos);
			LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_5);

			// blasting is a-go
			LL_TIM_SetCounter(TIM1, 0);
			LL_TIM_EnableCounter(TIM1);
		}
		void wait(uint16_t ticks) {
			if (delaying) return;
			// set the counter
			delaying = true;
			delay_counter = (ticks);
			// enable the ticker (happens every 3 ticks of tim1, compute based on width * 3 * amt)
			LL_TIM_SetCounter(TIM4, 0); // this isn't really necessary but may as well
			LL_TIM_SetAutoReload(TIM4, ticks); // 1 tick for the 0 state.
			LL_TIM_EnableCounter(TIM4);
		}

		void do_next() {
			const static uint16_t draw_ticks_table[] = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

			show = false;
			uint16_t drawn_pos = draw_ticks_table[pos];
			// set address pins
			if (pos == 12) {
				// We have finished clocking out a row, process buffer swaps
				if (should_swap) {
					active_buffer = !active_buffer;
					if (notify_when_swapped) xTaskNotifyFromISR(notify_when_swapped, 1, eSetValueWithOverwrite, NULL);
					should_swap = false;
					frames_without_refresh = 0;
				}
				else {
					frames_without_refresh += 1;
				}
				// Start the display again
				start_display();
				return;
			}
			strobe = true;
			// Thing
			GPIOB->ODR = (GPIOB->ODR & 0xfff0) | (row & 0x000f);
			++row;
			if (row < FB::stb_lines) {
				strobe = false;
				// Send off the next row
				blast_row();
				// Start the waiting.
				wait(drawn_pos);		
				return;
			}
			else if (++pos < 12) {
				strobe = false;
				// Set back to first row
				row = 0;
				// Blast and wait
				blast_row();
				wait(drawn_pos);		
				return;
			}
			else {
				strobe = false;
				// Alright, pos == 12, so run the wait code
				// Just wait, but force it to run us again.
				show = true;
				wait(drawn_pos);
			}
		}
	};

}

#endif
