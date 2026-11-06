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

		void prepare_stream(uint16_t i, uint8_t pos, uint8_t * bs) {
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

	private:

		class or_oob_ref {
			friend FrameBuffer;
			color_t *ref{};

			or_oob_ref() = default;
			or_oob_ref(color_t& ref) : ref(&ref) {}
			or_oob_ref(const or_oob_ref&) = delete;
			or_oob_ref(or_oob_ref&&) = delete;
			or_oob_ref& operator=(const or_oob_ref&) = delete;
			or_oob_ref& operator=(or_oob_ref&&) = delete;
		public:
			~or_oob_ref() = default;
			inline color_t operator=(color_t change) {
				if (ref) *ref = change;
				return change;
			}
			operator color_t() const {
				if (ref) return *ref;
				return {};
			}
		};

	public:
		inline or_oob_ref at(int x, int y) {
			if (on_screen(x, y)) {
				return Storage::_at(data, (uint16_t)x, (uint16_t)y);
			}
			return {};
		}

		inline color_t & at_unsafe(uint16_t x, uint16_t y) {
			return Storage::_at(data, x, y);
		}

		inline color_t at(int x, int y) const {
			if (on_screen(x, y)) {
				return Storage::_at(data, (uint16_t)x, (uint16_t)y);
			}
			return {};
		}

		inline color_t at_unsafe(uint16_t x, uint16_t y) const {
			return Storage::_at(data, x, y);
		}

		inline bool on_screen(int x, int y) const {
			return (x >= 0 && y >= 0) && (x < Width && y < Height);
		}

		static constexpr inline uint16_t width = Width;
		static constexpr inline uint16_t effective_width = Storage::EffectiveWidth;
		static constexpr inline uint16_t height = Height;
		static constexpr inline uint16_t stb_lines = 16;

	protected:
		alignas(uint32_t) color_t data[Width*Height];

	private:
		template<unsigned Pos>
		RAMFUNC void prepare_stream_bf(uint16_t i, uint8_t *bs) {
			uint16_t * lo = &data[i*Storage::EffectiveWidth].r;
			uint16_t * hi = lo + stb_lines*Storage::EffectiveWidth*3;

			// used to mask out bits while shifting in operand2
			uint32_t c0 = 1 | (1 << 16);
			// shifted over for hi row
			uint32_t c3 = c0 << 3;

			for (uint_fast16_t j = 0; j < Storage::EffectiveWidth / 2; ++j) {
				asm volatile (
					// Load 3 words from the first row (or 6 colors / 2 pixels) into r4-r6 using ldm
					"ldmia %[Lo]!, {r4,r5,r6}\n\t"
					// Load 3 words from the second row (or 6 colors / 2 pixels) into r8-r10 using ldm
					// (the registers are chosen to avoid conflicting with ones forced on us by
					// calling conventions; we can't ask the compiler to give them us for free since
					// we need 3 ascending registers...)
					"ldmia %[Hi]!, {r8,r9,r10}\n\t"
					// Select R0 and G0 to give us
					// |0*15|G0|0*15|R0| in r4 and the same thing but shifted << 3 in r8
					"and r4, %[C0], r4, ror %[Pos]\n\t"
					"and r8, %[C3], r8, ror %[Pos3]\n\t" // Pos3 = (Pos - 3) & 31; wrapped
					// Ditto for B0, R1 (little endian, so LSB is B0)
					"and r5, %[C0], r5, ror %[Pos]\n\t"
					"and r9, %[C3], r9, ror %[Pos3]\n\t"
					// Ditto for G1, B1
					"and r6, %[C0],  r6,  ror %[Pos]\n\t"
					"and r10, %[C3], r10, ror %[Pos3]\n\t"
					// Combine the top/bottom rows to get
					// |0*12|G0'|00|G0|0*12|R0'|00|R0
					// |0*12|R1'|00|R1|0*12|B0'|00|B0
					// |0*12|B1'|00|B1|0*12|G0'|00|G0
					// and so on in r4, r5, r6
					"orr r4, r4, r8\n\t"
					"orr r5, r5, r9\n\t"
					"orr r6, r6, r10\n\t"
					// Move G0 from the top of r4 into the bottom of r4 to
					// get 0 G0' R0' 0 G0 R0 to be the low 6 bits of r4; the upper
					// bits have still got G0'/G0 in them but we will ignore them
					"orr r4, r4, r4, lsr #15\n\t"
					// Move B0 from the bottom of r5 into the bottom of r4; we only
					// care about the low 6 bits still so the fact this further screws
					// up the upper half of the word is irrelevant since we'll bfi from 
					// this register at the end
					// (r1  b0) << 2 'slots into' correct place, upper bits ignored,
					// so now r4 is RGB'/RGB for pixel 0 on both rows
					"orr r4, r4, r5, lsl #2\n\t"
					// Move G1 into the upper half of r5, which is still unmodified, so
					// slots in after R1/R1'
					"orr r5, r5, r6, lsl #17\n\t"
					// Move B1 in too - now upper half of r5 is correct with RGB/RGB
					"orr r5, r5, r6, lsl #2\n\t"
					// Finally, copy in the low half of r4 and obliterate
					// the junk already there.
					"bfi r5, r4, #0, #6\n\t"
					// R5 is now ready.

					// Prepare actual value (with clock phases)
					"orr r5, r5, r5, lsl #8\n\t"
					"orr r5, r5, #0x40004000\n\t"
					// Store value 
					"str r5, [%[Bs]], #4\n\t"
					: [Lo]"=r"(lo), [Hi]"=r"(hi), [Bs]"=r"(bs)
					: "0"(lo), "1"(hi), "2"(bs), [Pos]"i"(Pos), [Pos3]"i"((Pos - 3) & 31), [C0]"r"(c0), [C3]"r"(c3)
					: "r4", "r5", "r6", "r8", "r9", "r10", "memory", "cc"
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
			active_dma_buffer = false;

			_get_active_buffer().prepare_stream(0, 0, dma_buffer[active_dma_buffer]);
			_get_active_buffer().prepare_stream(1, 0, dma_buffer[!active_dma_buffer]);

			blast_active_row_buffer();
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
			else show = true;
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
		bool active_dma_buffer = false;
		uint8_t pos = 0;
		uint16_t row = 0;
		// This is written to via 32-bit writes, and we want there to be at least one 16bit word
		// at the end with all zeroes, hence the +4.
		alignas(uint32_t) uint8_t dma_buffer[2][(FB::effective_width * 2) + 4] {};
		volatile TaskHandle_t notify_when_swapped = nullptr;

		// sanity guard on wait
		bool delaying = false;

		// counter for both interrupts to have fired; both the OE pulse + dma out
		bool show = false;

		// should swap buffers
		volatile bool should_swap = false;

		// pins
		gpio::Pin<GPIOB_BASE, 5> strobe;
		
		// clock is tied to tim2_ch3

		inline FB& RAMFUNC _get_active_buffer() {return active_buffer ? fb0 : fb1;}

		void RAMFUNC blast_active_row_buffer() {
			DMA2_Stream5->M0AR = (uint32_t)dma_buffer[active_dma_buffer];
			DMA2_Stream5->PAR  = (uint32_t)(&SIGN_DATA_Port->ODR);
			DMA2_Stream5->NDTR = sizeof dma_buffer[0];
			DMA2_Stream5->CR |= DMA_SxCR_EN;

			// blasting is a-go
			TIM1->CNT = 0;
			TIM1->CR1 |= TIM_CR1_CEN;
		}
		void RAMFUNC wait(uint16_t ticks) {
			if (delaying) return;
			// set the counter
			delaying = true;
			// enable the ticker (happens every 3 ticks of tim1, compute based on width * 3 * amt)
			TIM4->CNT = 0;
			TIM4->ARR = ticks;
			TIM4->CR1 |= TIM_CR1_CEN;
		}

		void RAMFUNC do_next() {
			strobe = true; // maximize strobe pulse time
			show = false;
			uint16_t drawn_pos = 2 << pos; 
			GPIOB->ODR = (GPIOB->ODR & 0xfff0) | (row & 0x000f);
			if (++row == FB::stb_lines) {
				row = 0;
				if (++pos == 12) {
					pos = 0;

					// about to send out next frame, activate masking now.
					TIM4->CCMR1 = (TIM4->CCMR1 & ~(TIM_CCMR1_CC1S_Msk | TIM_CCMR1_OC1M_Msk)) | (force_off ? LL_TIM_OCMODE_FORCED_INACTIVE : LL_TIM_OCMODE_PWM2);
				}
			}
			strobe = false;
			// swap dma buffers, row/pos now reflect content in active buffer
			active_dma_buffer = !active_dma_buffer;
			blast_active_row_buffer();
			// start OE pulse now
			wait(drawn_pos);

			auto next_row = row;
			auto next_pos = pos;
			if (++next_row == FB::stb_lines) {
				next_row = 0;
				if (++next_pos == 12) {
					next_pos = 0;
					// about to start reading next frame, swap main buffers
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
				}
			}

			// regardless, we can now prepare the non-sent-out dma buffer
			_get_active_buffer().prepare_stream(next_row, next_pos, dma_buffer[!active_dma_buffer]);
		}
	};

}

#endif
