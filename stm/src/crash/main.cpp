#include "../matrix.h"
#include "decode.h"
#include "main.h"
#include "mindraw.h"
#include "simplematrix.h"
#include "stm32f2xx_ll_cortex.h"
#include "stm32f2xx_ll_system.h"
#include "stm32f2xx_ll_utils.h"
#include <alloca.h>
#include <cstdio>

extern "C" {
	extern uint32_t _ecstack;
	extern uint8_t _svram;
	extern uint8_t _ecdata;
	extern uint8_t _scidata;
	extern uint8_t _ecbss;
	extern uint8_t _scdata;
	extern uint32_t g_pfnVectors_crash;
};

namespace crash {
	// Smaller matrix to give more space for our private data
	Matrix matrix;

	inline constexpr uint8_t mkcolor(uint8_t r, uint8_t g, uint8_t b) {
		return r | (g << 2) | (b << 4);
	}

	void delay_ms(uint32_t delay) {
		volatile uint32_t dummyread = SysTick->CTRL;  // get rid of countflag

		while (delay)
		{
			if((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0)
			{
				--delay;
			}
		}
	}

	void cmain(const char* errcode, uint32_t SP, uint32_t PC, uint32_t LR) {
		// Mask out all application interrupts
		__set_BASEPRI(3 << (8 - __NVIC_PRIO_BITS));
		// Change VTOR to our VTOR
		SCB->VTOR = (uint32_t)&g_pfnVectors_crash;

		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
		LL_TIM_DisableCounter(TIM1);
		LL_TIM_DisableCounter(TIM9);

		LL_FLASH_DisableDataCache();
		LL_FLASH_DisableInstCache();

		// Disable the matrix interrupts
		NVIC_DisableIRQ(DMA2_Stream5_IRQn);
		NVIC_DisableIRQ(TIM1_BRK_TIM9_IRQn);
		// Zero our BSS
		bzero(&_svram, &_ecbss - &_svram);
		// Load our constant data
		memcpy(&_scdata, &_scidata, &_ecdata - &_scdata);

		__DSB();
		__ISB();

		// Initialize our matrix
		matrix.init();

		// Setup systick
		LL_SYSTICK_DisableIT();
		LL_Init1msTick(SystemCoreClock);

		// Turn on interrupts
		NVIC_SetPriority(DMA2_Stream5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,0));
		NVIC_EnableIRQ(DMA2_Stream5_IRQn);

		NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,1)); // sequence after each other
		NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);

		draw::text(matrix, "MSign crashed!", 0, 6, mkcolor(3, 0, 0));
		draw::text(matrix, errcode, 0, 12, mkcolor(3, 3, 3));

		// Create a backtrace
		uint32_t backtrace[32];
		uint16_t bt_len = 32;

		decode::fill_backtrace(backtrace, bt_len, PC, LR, SP);

		// Create function name list (ironically using VLA here breaks the backtracer, but whatever)
		char symbols[bt_len][decode::max_length_size];

		decode::resolve_symbols(symbols, backtrace, bt_len);

		// Start display again
		matrix.start_display();

		while (true) {
			int y = 18;
			int i = 0;
			while (i < bt_len) {
				char buf[16];
				snprintf(buf, 16, "[%d] - ", i);
				uint16_t indent = draw::text(matrix, buf, 0, y, mkcolor(3, 2, 0));
				snprintf(buf, 16, "0x%08x", backtrace[i]);
				draw::text(matrix, buf, indent, y, mkcolor(1, 1, 3));
				draw::text(matrix, symbols[i], indent, y+6, mkcolor(3, 3, 3));
				++i; y += 12;

				if (y > 64) {
					// Wait for a while
					delay_ms(2500);
					draw::rect(matrix, 0, 12, 128, 64, 0);
					y = 18;
				}
			}
			delay_ms(4000);
			draw::rect(matrix, 0, 12, 128, 64, 0);
			// TODO: show rect
		}
	}
	
	// Transfer control to the crash handler.
	//
	// `errcode` must either be null or point to memory outside of the crash handler's region (i.e.
	// outside VRAM)
	//
	// This function does not return.
	[[noreturn]] void call_crash_main(const char *errcode, uint32_t SP, uint32_t PC, uint32_t LR) {
		// Set the stack pointer to the end of the crash handler's stack
		// 
		// Since we're running from either an interrupt handler or a task, we don't set the MSP, rather
		// we just set the current stack pointer and use that.
		
		asm volatile (
			"mov sp, %0\n\t"
			: 
			: "r" (&_ecstack)
			: "memory"
		);

		// Now we just jump directly to the crash main
		asm volatile (
			"b.n %0\n\t"
			:
			: "i" (&cmain)
			: "memory"
		);
	}

	[[noreturn]] void panic(const char* errcode) {
		uint32_t pc, lr, sp;
		asm volatile (
			"mov %0, pc\n\t"
			"mov %1, lr\n\t"
			"mov %2, sp\n\t" :
			"=r" (pc), "=r" (lr), "=r" (sp)
		);
		call_crash_main(errcode, sp, pc, lr);
	}
}
