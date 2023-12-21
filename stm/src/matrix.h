#ifndef MATRIX_H
#define MATRIX_H

#include "crash/main.h"
#include "gpio.h"
#include <stdint.h>
#include <string.h>
#include "nvic.h"
#include "pins.h"
#include <FreeRTOS.h>
#include <task.h>
#include "color.h"
#include "ramfunc.h"

#ifdef USE_F2
#include <stm32f2xx_ll_tim.h>
#include <stm32f2xx_ll_bus.h>
#include <stm32f2xx_ll_gpio.h>
#include <stm32f2xx_ll_dma.h>
#endif

#ifdef USE_F4
#include <stm32f4xx_ll_tim.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_dma.h>
#endif

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

		void RAMFUNC prepare_stream(uint16_t i, uint8_t pos, uint8_t * bs) {
			switch (pos) {
				case 0:
					prepare_stream_bf<0>(i, bs);
					return;
				case 1:
					prepare_stream_bf<1>(i, bs);
					return;
				case 2:
					prepare_stream_bf<2>(i, bs);
					return;
				case 3:
					prepare_stream_bf<3>(i, bs);
					return;
				case 4:
					prepare_stream_bf<4>(i, bs);
					return;
				case 5:
					prepare_stream_bf<5>(i, bs);
					return;
				case 6:
					prepare_stream_bf<6>(i, bs);
					return;
				case 7:
					prepare_stream_bf<7>(i, bs);
					return;
				case 8:
					prepare_stream_bf<8>(i, bs);
					return;
				case 9:
					prepare_stream_bf<9>(i, bs);
					return;
				case 10:
					prepare_stream_bf<10>(i, bs);
					return;
				case 11:
					prepare_stream_bf<11>(i, bs);
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

		static constexpr inline uint16_t width = Width;
		static constexpr inline uint16_t effective_width = Storage::EffectiveWidth;
		static constexpr inline uint16_t height = Height;
		static constexpr inline uint16_t stb_lines = 16;

	protected:
		alignas(uint32_t) color_t data[Width*Height];

		color_t junk; // used as failsafe for read/write out of bounds

	private:
		template<unsigned Pos>
		RAMFUNC void prepare_stream_bf(uint16_t i, uint8_t *bs) {
			uint16_t * lo = &data[i*Storage::EffectiveWidth].r;
			uint16_t * hi = lo + stb_lines*Storage::EffectiveWidth*3;

			// used to mask out bits while shifting in operand2
			uint32_t c0 = 1 | (1 << 16);

			// scratch register to store word to write
			uint32_t scratch;

			for (uint_fast16_t j = 0; j < Storage::EffectiveWidth / 2; ++j) {
				asm volatile (
#define PROCESS_BITS(target) \
					/* Select G0 & R0 out from the first word, leaving r1 looking like
					   |0*15|G|0*15|R| */ \
					"and r1, %[C0], r1, lsr %[Pos]\n\t" \
					/* Select R1 & B0 out from the second word, leaving r2 looking like
					   |0*15|R|0*15|B| */ \
					"and r2, %[C0], r2, lsr %[Pos]\n\t" \
					/* Move G0 from the top of r1 into the bottom of r1 -- this does not
					   set extra bits as the upper 15 bits of r1 are always zero. */ \
					"orr r1, r1, r1, lsr #15\n\t" \
					/* Move B0 from the bottom of r2 into the bottom of r1 -- the upper bits
					   of r1 will be corrupted by this but they will be ignored during a final
					   bit field insert */ \
					"orr r1, r1, r2, lsl #2\n\t" \
					/* At this point, R1 is |junk*16|0*13|BGR| for the first pixel.
					   Now, prepare the second pixel. Select B1 & G1 out of r3,
					   leaving r3 looking like
					   |0*15|B|0*15|G| */ \
					"and r3, %[C0], r3, lsr %[Pos]\n\t" \
					/* Move G1&B1 into the upper half of r2 after R1 which is already there.
					   These will corrupt the bottom 3 bits of r2 but those will be overwritten
					   during the bit field insert. */ \
					"orr r2, r2, r3, lsl #17\n\t" \
					/* Instead of writing the result into r2, write it into the scratch register
					   to later get orred */ \
					"orr " target ", r2, r3, lsl #2\n\t" \
					/* Finally, copy the RGB0 part of r1 overwriting the bottom part of r2 to leave r2 in the desired state. */ \
					"bfi " target ", r1, #0, #3\n\t"

					// Load 3 words from the first row (or 6 colors / 2 pixels) into r1-r3 using ldm
					"ldmia %[Lo]!, {r1,r2,r3}\n\t"
					// Process them into the temp register
					PROCESS_BITS("%[Temp]")
					// Load 3 words from the second row (or 6 colors / 2 pixels) into r1-r3 using ldm
					"ldmia %[Hi]!, {r1,r2,r3}\n\t"
					// Process them into r2
					PROCESS_BITS("r2")

					// Prepare actual value
					"orr %[Temp], %[Temp], r2, lsl #3\n\t"
					"orr %[Temp], %[Temp], %[Temp], lsl #8\n\t"
					"orr %[Temp], %[Temp], #0x40004000\n\t"
					// Store value 
					"str %[Temp], [%[Bs]], #4\n\t"
					: [Lo]"=r"(lo), [Hi]"=r"(hi), [Bs]"=r"(bs), [Temp]"=r"(scratch)
					: "0"(lo), "1"(hi), "2"(bs), [Pos]"i"(Pos), [C0]"r"(c0)
					: "r1", "r2", "r3", "cc"
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
			return const_cast<color_t &>(_at((const color_t *)data, x, y));
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

		Matrix() : fb0(), fb1() {}

		void init() {
			// Enable clocks
			LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);	 // timer
			LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);
			LL_AHB1_GRP1_EnableClock(SIGN_GPIO_PERIPH | LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_DMA2); // gpios

			// Setup the timer.
			LL_TIM_InitTypeDef tim_init = {0};
			
			// roughly 20 MHz (21 in new build)
#if defined(USE_F2)
			tim_init.Prescaler  = 3;
#elif defined(USE_F4)
			tim_init.Prescaler  = 4;
#endif
			tim_init.Autoreload = 1;
			tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
			LL_TIM_Init(TIM1, &tim_init);

#if defined(USE_F2)
			tim_init.Prescaler  = 2;
#elif defined(USE_F4)
			tim_init.Prescaler  = 4;
#endif
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
			DMA2_Stream5->CR = LL_DMA_CHANNEL_6 | DMA_SxCR_PL | DMA_SxCR_MINC |
				               LL_DMA_MDATAALIGN_HALFWORD | LL_DMA_MBURST_INC4 |
							   LL_DMA_DIRECTION_MEMORY_TO_PERIPH | DMA_SxCR_TCIE | DMA_SxCR_TEIE;
			DMA2_Stream5->FCR = DMA_SxFCR_DMDIS | LL_DMA_FIFOTHRESHOLD_1_2;

			// Setup the first line of EXTI as a sw irq
			EXTI->EMR |= EXTI_EMR_EM0;
			EXTI->IMR |= EXTI_IMR_IM0;
		}

		void RAMFUNC start_display() {
			// Set config values back to sane defaults
			pos = 0;
			row = 0;
			show = true;
			delaying = false;
			should_swap = false;
			delay_counter = 0;
			// update timer mode to blank screen / proper
			TIM4->CCMR1 = (TIM4->CCMR1 & ~(TIM_CCMR1_CC1S_Msk | TIM_CCMR1_OC1M_Msk)) | (force_off ? LL_TIM_OCMODE_FORCED_INACTIVE : LL_TIM_OCMODE_PWM2);
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
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		}

		void swap_buffers_from_isr() {
			notify_when_swapped = nullptr;
			should_swap = true;
			while (should_swap) {;}
		}

		FB& get_inactive_buffer() {return active_buffer ? fb1 : fb0;}
		const FB& get_active_buffer() {return active_buffer ? fb0 : fb1;}

		void RAMFUNC dma_finish() {
			// blast is finished, stop dma + timer output
			TIM1->CR1 &= ~TIM_CR1_CEN;
			DMA2_Stream5->CR &= ~DMA_SxCR_EN;
			// check show
			if (show) do_next();
			else show = true;
		}

		void RAMFUNC tim_elapsed() {
			if (!delaying) return;
			// we don't bother clearing counter enable since OPM will do it for us.
			delaying = false;
			if (show) do_next();
			else {
				show = true;
			}
		}

		void sw_trap_fired() {
			if (notify_when_swapped) {
				vTaskNotifyGiveFromISR(notify_when_swapped, nullptr);
				notify_when_swapped = nullptr;
			}
		}

		int16_t frames_without_refresh = 0;
		volatile bool force_off = false;

	private:
		FB fb0, fb1;
		bool active_buffer = false;
		uint8_t pos = 0;
		uint16_t row = 0;
		// This is written to via 32-bit writes, and we want there to be at least one 16bit word
		// at the end with all zeroes, hence the +4.
		alignas(uint32_t) uint8_t dma_buffer[(FB::effective_width * 2) + 4] {};
		volatile TaskHandle_t notify_when_swapped = nullptr;

		// impl for the wait system
		uint16_t delay_counter = 0;
		bool delaying = false;

		// show variable
		bool show = false;

		// should swap buffers
		volatile bool should_swap = false;

		// pins
		gpio::Pin<GPIOB_BASE, 5> strobe;
		
		// clock is tied to tim2_ch3

		inline FB& RAMFUNC _get_active_buffer() {return active_buffer ? fb0 : fb1;}

		void RAMFUNC blast_row() {
			DMA2_Stream5->M0AR = (uint32_t)dma_buffer;
			DMA2_Stream5->PAR  = (uint32_t)(&SIGN_DATA_Port->ODR);
			DMA2_Stream5->NDTR = sizeof dma_buffer;

			// make sure the active buffer is ready
			_get_active_buffer().prepare_stream(row, pos, dma_buffer);
			DMA2_Stream5->CR |= DMA_SxCR_EN;

			// blasting is a-go
			TIM1->CNT = 0;
			TIM1->CR1 |= TIM_CR1_CEN;
		}
		void RAMFUNC wait(uint16_t ticks) {
			if (delaying) return;
			// set the counter
			delaying = true;
			delay_counter = (ticks);
			// enable the ticker (happens every 3 ticks of tim1, compute based on width * 3 * amt)
			TIM4->CNT = 0;
			TIM4->ARR = ticks;
			TIM4->CR1 |= TIM_CR1_CEN;
		}

		void RAMFUNC do_next() {
			show = false;
			uint16_t drawn_pos = 2 << pos; 
			// set address pins
			if (pos == 12) {
				// We have finished clocking out a row, process buffer swaps
				if (should_swap) {
					active_buffer = !active_buffer;
					if (notify_when_swapped) EXTI->SWIER = 1; // Trigger EXTI 1, which will tell the RTOS to swap after this high prio interrupt finishes.
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
