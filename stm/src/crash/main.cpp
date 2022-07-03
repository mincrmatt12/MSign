#include "../matrix.h"
#include "decode.h"
#include "main.h"
#include "mindraw.h"
#include "rtos.h"
#include "simplematrix.h"
#include "../tskmem.h"
#include "stm32f2xx_ll_cortex.h"
#include "stm32f2xx_ll_system.h"
#include "../ui.h"
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

	constexpr inline uint32_t page_delay = 5000;

	inline constexpr uint8_t mkcolor(uint8_t r, uint8_t g, uint8_t b) {
		return r | (g << 2) | (b << 4);
	}

	void delay_ms(uint32_t delay, bool cancellable=true) {
		volatile uint32_t dummyread = SysTick->CTRL;  // get rid of countflag

		int state = 0;

		while (delay)
		{
			if((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0)
			{
				if (state == 1) state = 2;
				--delay;
			}

			bool ispressed = (UI_BUTTON_PORT->IDR & (1 << ui::Buttons::POWER)) && cancellable;

			switch (state) {
				case 0: // waiting for pressed
					if (ispressed) state = 1;
					break;
				case 1:
					if (!ispressed) state = 0;
					break;
				case 2:
					if (!ispressed) return;
					break;
			}
		}
	}

	int y = 18;

	uint16_t print_line(const char* txt, uint8_t color, uint16_t indent, bool advance=true) {
		if (y > 64) {
			delay_ms(page_delay);
			draw::rect(matrix, 0, 12, 128, 64, 0);
			y = 18;
		}
		auto result = draw::text(matrix, txt, indent, y, color);
		if (advance) y += 6;
		return result;
	}

	void print_wrapped_lines(const char *txt, uint8_t color, uint16_t indent) {
		int remaining = strlen(txt), pos = 0;
		while (remaining > 0) {
			size_t printed = draw::break_line(txt + pos, indent);
			print_line(txt + pos, color, indent);
			pos += printed;
			remaining -= printed;
		}
	}

	void show_backtrace_for(const char * title, uint32_t SP, uint32_t PC, uint32_t LR) {
		// Create a backtrace
		uint32_t backtrace[32];
		uint16_t bt_len = 32;

		decode::fill_backtrace(backtrace, bt_len, PC, LR, SP);

		// Create function name list (ironically using VLA here breaks the backtracer, but whatever)
		char symbols[bt_len][decode::max_length_size];

		decode::resolve_symbols(symbols, backtrace, bt_len);

		// DRAW STACKTRACE
		print_line(title, mkcolor(1, 3, 1), 0);
		for (int i = 0; i < bt_len; ++i) {
			char buf[16];
			snprintf(buf, 16, "%d - ", i);
			uint16_t indent = print_line(buf, mkcolor(3, 2, 0), 0, false);
			snprintf(buf, 16, "0x%08x", backtrace[i]);
			print_line(buf, mkcolor(1, 1, 3), indent);
			print_wrapped_lines(symbols[i], mkcolor(3, 3, 3), indent);
		}

		y = 65; // force page end
	}

	void cmain(const char* errcode, uint32_t SP, uint32_t PC, uint32_t LR) {
		// Mask out all application interrupts
		__set_BASEPRI(3 << (8 - __NVIC_PRIO_BITS));
		// Change VTOR to our VTOR
		SCB->VTOR = (uint32_t)&g_pfnVectors_crash;
		// Try to exit an exception handler, if possible. (todo)
		
		// Check if sleep mode is enabled -- if so, just reset immediately.
		if (RTC->BKP4R) {
			NVIC_SystemReset();
			while (1) {;}
		}

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

		// Setup GPIO for power button
		
		LL_AHB1_GRP1_EnableClock(UI_BUTTON_PERIPH);

		LL_GPIO_InitTypeDef it{};

		it.Mode = LL_GPIO_MODE_INPUT;
		it.Pull = LL_GPIO_PULL_DOWN;
		it.Speed = LL_GPIO_SPEED_FREQ_LOW;
		it.Pin = (1 << ui::Buttons::POWER);

		LL_GPIO_Init(UI_BUTTON_PORT, &it); // setup to check for power button presses to advance through screens

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

		if (PC == 0xffff'ffff) {
			matrix.start_display();
			goto reboot;
		}

		{
			// Start display again
			matrix.start_display();

			while (true) {
				show_backtrace_for("Traceback (recent calls first)", SP, PC, LR);
				print_line("=== CONTEXT ===", mkcolor(1, 3, 1), 0);

				// Are we in a fault handler?
				{
					uint8_t active_isr = __get_IPSR() & 0xff;
					if (active_isr) {
						// We are, check the CFSR bits
						switch (active_isr) {
							case 5:
								// bus fault
								{
									print_line("bus fault:", mkcolor(3, 0, 0), 1);
									if (SCB->CFSR & SCB_CFSR_STKERR_Msk) print_line("during exception stacking", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_UNSTKERR_Msk) print_line("during exception unstacking", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_IBUSERR_Msk) print_line("during inst fetch", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_PRECISERR_Msk || SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk) print_line("during data fetch", mkcolor(1, 1, 1), 2);

									if (SCB->CFSR & SCB_CFSR_BFARVALID_Msk) {
										char buf[32];
										snprintf(buf, 32, "while accessing 0x%08x", SCB->BFAR);
										print_line(buf, mkcolor(1, 1, 3), 2);
									}
								}
								break;
							case 6:
								// usage fault
								{
									print_line("usage fault:", mkcolor(3, 0, 0), 1);
									if (SCB->CFSR & SCB_CFSR_NOCP_Msk) print_line("no coprocessor", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_UNDEFINSTR_Msk) print_line("undefined instruction", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_INVSTATE_Msk) print_line("invalid isa state", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_INVPC_Msk) print_line("invalid exception return", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_UNALIGNED_Msk) print_line("unaligned load/store", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_DIVBYZERO_Msk) print_line("divide by 0", mkcolor(1, 1, 1), 2);
									else print_line("unknown", mkcolor(3, 3, 3), 2);
								}
								break;
							case 4:
								// memory manage
								{
									print_line("memory protection fault:", mkcolor(3, 0, 0), 1);
									if (SCB->CFSR & SCB_CFSR_IACCVIOL_Msk) print_line("during inst fetch", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_DACCVIOL_Msk) print_line("during data fetch", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_MSTKERR_Msk) print_line("during exception stacking", mkcolor(1, 1, 1), 2);
									else if (SCB->CFSR & SCB_CFSR_MUNSTKERR_Msk) print_line("during exception unstacking", mkcolor(1, 1, 1), 2);

									if (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) {
										char buf[32];
										snprintf(buf, 32, "while accessing 0x%08x", SCB->MMFAR);
										print_line(buf, mkcolor(1, 1, 3), 2);
									}
								}
								break;
							default:
								// other interrupt
								{
									char buf[32];
									snprintf(buf, 32, "from ISR %d", active_isr);
									print_line(buf, mkcolor(3, 0, 0), 1);
								}
								break;
						}
					}

					{
						// Try to check the active thread
						if (rtos::is_rtos_running()) {
							const char* tname = rtos::guess_active_thread_name();
							if (!tname) {
								print_line("corrupted RTOS state", mkcolor(3, 0, 0), 1);
							}
							else {
								char buf[32];
								snprintf(buf, 32, "from thread %s", tname);
								print_line(buf, mkcolor(0, 3, 0), 1);

							}
							// Try to dump other tasks
							y = 65;
							uint32_t tSP, tPC, tLR;
							rtos::get_task_regs(*reinterpret_cast<uint32_t **>(&tskmem::srvc), tPC, tSP, tLR);
							show_backtrace_for("Traceback for servicer", tSP, tPC, tLR);
							rtos::get_task_regs(*reinterpret_cast<uint32_t **>(&tskmem::screen), tPC, tSP, tLR);
							show_backtrace_for("Traceback for screen", tSP, tPC, tLR);
							rtos::get_task_regs(*reinterpret_cast<uint32_t **>(&tskmem::dbgtim), tPC, tSP, tLR);
							show_backtrace_for("Traceback for dbgtim", tSP, tPC, tLR);
						}
					}


					// Reset printer
					delay_ms(page_delay);
					draw::rect(matrix, 0, 12, 128, 64, 0);
					y = 18;
				}
			}
		}
reboot:
		for (int tim = 8; tim >= 0; --tim) {
			draw::rect(matrix, 0, 12, 128, 64, 0);
			draw::text(matrix, "rebooting:", 0, 18, 0xff);
			draw::rect(matrix, 0, 20, tim*16, 26, mkcolor(1, 2, 3));
			delay_ms(1000, false);
		}

		NVIC_SystemReset();
		while (1) {;}
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
				"ldr r12, =%0\n\t"
				"bx r12\n\t"
				:
				: "i" (&cmain)
				: "memory"
				);
	}

	[[noreturn]] void panic_nonfatal(const char* errcode) {
		call_crash_main(errcode, 0, 0xffff'ffff, 0);
	}

	[[noreturn]] void panic_from_isr(const char *errcode, uint32_t *SP_at_isr) {
		uint32_t PC = SP_at_isr[6];
		uint32_t LR = SP_at_isr[5];
		bool aligned = SP_at_isr[7] & (1 << 9);
		uint32_t SP = (uint32_t)(&SP_at_isr[8]) + aligned * 4;
		call_crash_main(errcode, SP, PC, LR);
	}

}

extern "C" void msign_panic(const char *msg) {
	crash::panic(msg);
}
