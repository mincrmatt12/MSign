#include "nvic.h"
#include "crash/main.h"
#include "pins.h"

#include "stm32f2xx.h"

#include "srv.h"
#include "stm32f2xx_hal.h"
#include "stm32f2xx_ll_usart.h"
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

	NVIC_SetPriority(ESP_USART_IRQ_NAME, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),6,0));
	NVIC_EnableIRQ(ESP_USART_IRQ_NAME);

	NVIC_SetPriority(UsageFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),3,0));
	NVIC_EnableIRQ(UsageFault_IRQn);

	NVIC_SetPriority(BusFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),3,0));
	NVIC_EnableIRQ(BusFault_IRQn);

	NVIC_SetPriority(MemoryManagement_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),3,0));
	NVIC_EnableIRQ(MemoryManagement_IRQn);

	SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk;

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
		crash::panic("RxNoHandle");
	}
}

extern "C" void ESP_USART_IRQ_HANDLER() {
	// just clears error flags
	if (LL_USART_IsActiveFlag_FE(ESP_USART)) LL_USART_ClearFlag_FE(ESP_USART);
	if (LL_USART_IsActiveFlag_PE(ESP_USART)) LL_USART_ClearFlag_PE(ESP_USART);
	if (LL_USART_IsActiveFlag_ORE(ESP_USART)) LL_USART_ClearFlag_ORE(ESP_USART);
	if (LL_USART_IsActiveFlag_NE(ESP_USART)) LL_USART_ClearFlag_NE(ESP_USART);
}

extern "C" void NVIC_SRV_TX_IRQ_HANDLER() {
	if (NVIC_SRV_TX_ACTV(UART_DMA)) {
		NVIC_SRV_TX_CLRF(UART_DMA);
		servicer.dma_finish(false);
	}
}

extern "C" void xPortSysTickHandler();

extern "C" void SysTick_Handler() {
	if (matrix.frames_without_refresh >= 140) {
		crash::panic("screen refresh watchdog");
	}
	timekeeper.systick_handler();
	xPortSysTickHandler();
}

extern "C" void MemManage_Handler() __attribute__((naked));
extern "C" void MemManage_Handler() {
	asm volatile (
		"ldr r0, =%0\n\t" // load the string into the first parameter

		"tst lr, #4\n\t"  // check bit 3 of the LR to see which stack is in use
		"ite eq\n\t"
		"mrseq r1, msp\n\t" // if it's clear, use the main stack
		"mrsne r1, psp\n\t" // otherwise, use the process stack

		"ldr r3, =%1\n\t"   // load the address of the crash routine
		"bx r3\n\t" :       // jump to it
		: "i" ("MemManage (xn fail)"), "i" (&crash::panic_from_isr)
		: "memory"
	);
}

extern "C" void UsageFault_Handler() __attribute__((naked));
extern "C" void UsageFault_Handler() {
	asm volatile (
		"ldr r0, =%0\n\t"
		"tst lr, #4\n\t"
		"ite eq\n\t"
		"mrseq r1, msp\n\t"
		"mrsne r1, psp\n\t"
		"ldr r3, =%1\n\t" 
		"bx r3\n\t" :
		: "i" ("UsageFault"), "i" (&crash::panic_from_isr)
		: "memory"
	);
}

extern "C" void BusFault_Handler() __attribute__((naked));
extern "C" void BusFault_Handler() {
	asm volatile (
		"ldr r0, =%0\n\t"
		"tst lr, #4\n\t"
		"ite eq\n\t"
		"mrseq r1, msp\n\t"
		"mrsne r1, psp\n\t"
		"ldr r3, =%1\n\t" 
		"bx r3\n\t" :
		: "i" ("BusFault"), "i" (&crash::panic_from_isr)
		: "memory"
	);
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
