#include "../matrix.h"
#include "main.h"
#include "mindraw.h"
#include "simplematrix.h"
#include "stm32f2xx_hal_cortex.h"

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

	void cmain(const char* errcode, uint32_t SP, uint32_t PC, uint32_t LR) {
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
		LL_TIM_DisableCounter(TIM1);
		LL_TIM_DisableCounter(TIM9);

		// Mask out all application interrupts
		__set_BASEPRI(3 << (8 - __NVIC_PRIO_BITS));
		// Zero our BSS
		bzero(&_svram, &_ecbss - &_svram);
		// Load our constant data
		memcpy(&_scdata, &_scidata, &_ecdata - &_scdata);
		// Change VTOR to our VTOR
		SCB->VTOR = (uint32_t)&g_pfnVectors_crash;

		// Initialize our matrix
		matrix.init();

		// Turn on interrupts
		NVIC_SetPriority(DMA2_Stream5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,0));
		NVIC_EnableIRQ(DMA2_Stream5_IRQn);

		NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,1)); // sequence after each other
		NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);

		// Show error screen
		for (int i = 0; i < 256; ++i) matrix.frame[i] = i;

		//matrix.color(5, 5) = 0xff;

		//draw::text(matrix, "MSign crashed!", 0, 6, mkcolor(3, 0, 0));
		//draw::text(matrix, errcode, 0, 6, mkcolor(0, 3, 3));

		// Start display again
		//matrix.start_display();

		// Loop forever
		while (1) {
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
