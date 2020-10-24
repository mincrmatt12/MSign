#include "nvic.h"
#include "pins.h"

#include "stm32f2xx.h"
#include "stm32f2xx_hal.h"
#include "stm32f2xx_hal_tim.h"

#include "srv.h"
#include "tasks/timekeeper.h"
#include "matrix.h"
#include "draw.h"
#include "fonts/lcdpixel_6.h"

#include <FreeRTOS.h>

extern matrix_type matrix;
extern srv::Servicer servicer;
extern tasks::Timekeeper timekeeper;

void nvic::init() {
	NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

	NVIC_SetPriority(DMA2_Stream5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),4,0));
	NVIC_EnableIRQ(DMA2_Stream5_IRQn);

	NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),4,1)); // sequence after each other
	NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);

	NVIC_SetPriority(NVIC_SRV_TX_IRQ_NAME, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),6,0));
	NVIC_EnableIRQ(NVIC_SRV_TX_IRQ_NAME);

	NVIC_SetPriority(NVIC_SRV_RX_IRQ_NAME, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5,0));
	NVIC_EnableIRQ(NVIC_SRV_RX_IRQ_NAME);

	NVIC_SetPriority(UsageFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),3,0));
	NVIC_EnableIRQ(UsageFault_IRQn);

	NVIC_SetPriority(BusFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),3,0));
	NVIC_EnableIRQ(BusFault_IRQn);

	SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;

	__set_BASEPRI(0U);
}

extern "C" void DMA2_Stream5_IRQHandler() {
	if (LL_DMA_IsActiveFlag_TC5(DMA2)) {
		LL_DMA_ClearFlag_TC5(DMA2);
		matrix.dma_finish();
	}
}

extern "C" void TIM1_BRK_TIM9_IRQHandler() {
	if (LL_TIM_IsActiveFlag_UPDATE(TIM9)) {
		LL_TIM_ClearFlag_UPDATE(TIM9);
		matrix.tim_elapsed();
	}
}

extern "C" void NVIC_SRV_RX_IRQ_HANDLER() {
	if (NVIC_SRV_RX_ACTV(UART_DMA)) {
		NVIC_SRV_RX_CLRF(UART_DMA);
		servicer.dma_finish(true);
	}
	else if (NVIC_SRV_RXE_ACTV(UART_DMA)) {
		servicer.dma_finish(true);
	}
	else {
		nvic::show_error_screen("RxNoHandle");
	}
}

extern "C" void NVIC_SRV_TX_IRQ_HANDLER() {
	if (NVIC_SRV_TX_ACTV(UART_DMA)) {
		NVIC_SRV_TX_CLRF(UART_DMA);
		servicer.dma_finish(false);
	}
}

extern "C" void xPortSysTickHandler();

extern "C" void SysTick_Handler() {
	timekeeper.systick_handler();
	xPortSysTickHandler();
}

// hard fault handler renderer...

namespace {
	void draw_hardfault_screen(int remain_loop) {
		draw::fill(matrix.get_inactive_buffer(), 0, 0, 0);
		draw::text(matrix.get_inactive_buffer(), "MSign crashed!", font::lcdpixel_6::info, 0, 6, 4095, 1, 1);
		draw::text(matrix.get_inactive_buffer(), "rebooting in:", font::lcdpixel_6::info, 0, 12, 128_c, 128_c, 128_c);

		draw::rect(matrix.get_inactive_buffer(), 0, 24, remain_loop / 2, 32, 100_c, 100_c, 4095);
	}
}

[[noreturn]] void nvic::show_error_screen(const char * errcode) {
	if (!xPortIsInsideInterrupt()) vTaskSuspendAll();
	__set_BASEPRI(3 << (8 - __NVIC_PRIO_BITS));
	// boost priority of timer/dma
	NVIC_SetPriority(DMA2_Stream5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,0));
	NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,1)); // sequence after each other
	// there was a hardfault... delay for a while so i know
	for (int j = 0; j < 128; ++j) {
		draw_hardfault_screen(j);
		draw::text(matrix.get_inactive_buffer(), errcode, font::lcdpixel_6::info, 0, 20, 4095, 128_c, 0);
		matrix.swap_buffers_from_isr();
	}

	NVIC_SystemReset();
	while(1) {;}
}

extern "C" void UsageFault_Handler() {
	nvic::show_error_screen("UsageFault");
}
extern "C" void BusFault_Handler() {
	nvic::show_error_screen("BusFault");
}

extern "C" void HardFault_Handler() __attribute__((naked));
extern "C" void HardFault_Handler() {
	// Put onto the running stack and bkpt
	asm volatile (
			"tst lr, #4\n"
			"ite eq \n"
			"mrseq r0, msp\n"
			"mrsne r0, psp\n"
			"mov sp, r0\n"
			"bkpt #1\n"
	);
}
