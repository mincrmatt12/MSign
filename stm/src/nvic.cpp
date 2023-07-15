#include "nvic.h"
#include "crash/main.h"
#include "pins.h"
#include "ramfunc.h"

#ifdef USE_F2
#include <stm32f2xx.h>
#include <stm32f2xx_hal.h>
#include <stm32f2xx_ll_usart.h>
#include <stm32f2xx_ll_bus.h>
#endif
#ifdef USE_F4
#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include <stm32f4xx_ll_usart.h>
#include <stm32f4xx_ll_bus.h>
#endif

#include "srv.h"
#include "tasks/timekeeper.h"
#include "matrix.h"
#include "draw.h"
#include "fonts/lcdpixel_6.h"

#include <FreeRTOS.h>

extern matrix_type matrix;
extern srv::Servicer servicer;
extern tasks::Timekeeper timekeeper;
extern "C" void (*const g_pfnVectors[80 + 16])(void);
extern "C" uint8_t _srdata[];
extern "C" uint8_t _erdata[];

namespace nvic {
	alignas(0x400) __attribute__((section(".ram_isr_vector"))) void (*volatile ram_isr_table[80 + 16])(void) = {};

	void RAMFUNC unstalled_proxy_ram_irqhandler() {
		// Wait for flash to be finished
		while (FLASH->SR & FLASH_SR_BSY) {;}
		// Find original vector in flash table
		g_pfnVectors[__get_IPSR() & 0xff]();
	}
}

void nvic::setup_isrs_for_flash(bool enable) {
	__disable_fault_irq();
	if (enable) {
		for (int i = 1; i < sizeof(ram_isr_table) / 4; ++i) {
			uint32_t addr = (uint32_t)ram_isr_table[i];
			if (addr >= 0x0800'0000 && addr <= 0x1000'0000) {
				ram_isr_table[i] = unstalled_proxy_ram_irqhandler;
			}
		}
	}
	else {
		memcpy((void *)ram_isr_table, (void *)g_pfnVectors, sizeof g_pfnVectors);
	}
	__enable_fault_irq();
}

void nvic::init() {
	// Enable clock to SYSCFG
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);

	// Prepare RAM ISR table
	__disable_fault_irq();
	SYSCFG->MEMRMP = 0b11; // remap 0x0 to sram
	memcpy((void *)ram_isr_table, (const void *)g_pfnVectors, sizeof g_pfnVectors);
	SCB->VTOR = (uint32_t)(ram_isr_table);
	__enable_fault_irq();

	NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

	NVIC_SetPriority(DMA2_Stream5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,0));
	NVIC_EnableIRQ(DMA2_Stream5_IRQn);

	NVIC_SetPriority(TIM4_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2,1)); 
	NVIC_EnableIRQ(TIM4_IRQn);

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

	NVIC_SetPriority(EXTI0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),4,0));
	NVIC_EnableIRQ(EXTI0_IRQn);

	SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk;

	__set_BASEPRI(0U);
}

extern "C" void RAMFUNC DMA2_Stream5_IRQHandler() {
	if (LL_DMA_IsActiveFlag_TC5(DMA2)) {
		LL_DMA_ClearFlag_TC5(DMA2);
		matrix.dma_finish();
	}
}

extern "C" void RAMFUNC TIM4_IRQHandler() {
	if (TIM4->SR & TIM_SR_UIF) {
		TIM4->SR = ~TIM_SR_UIF;
		matrix.tim_elapsed();
	}
}

extern "C" void EXTI0_IRQHandler() {
	EXTI->PR |= EXTI_PR_PR0;
	matrix.sw_trap_fired();
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

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName) {
	char msg[32];
	snprintf(msg, 32, "stack over %s", pcTaskName);
	crash::panic(msg);
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
		"bx r3\n\t"         // jump to it
		".ltorg\n\t" :
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
		"bx r3\n\t"
		".ltorg\n\t" :
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
		"bx r3\n\t"
		".ltorg\n\t" :
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
