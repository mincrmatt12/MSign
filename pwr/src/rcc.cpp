#include "rcc.h"

#include <stdint.h>
#include <stm32g0xx_ll_rcc.h>
#include <stm32g0xx_ll_system.h>
#include <stm32g0xx_ll_utils.h>
#include <stm32g0xx_ll_cortex.h>
#include <stm32g0xx_ll_bus.h>

extern "C" {
	extern void (*__preinit_array_start [])(void);
	extern void (*__preinit_array_end [])(void);
	extern void (*__init_array_start [])(void);
	extern void (*__init_array_end [])(void);

	// required by USART init routines
	const uint32_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
	const uint32_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};
}

void rcc::init() {
	LL_RCC_HSI_Enable();
	// Wait till HSI is ready
	while (LL_RCC_HSI_IsReady() != 1) {}

	// Setup ahb/apb prescaler and switch to PLL for system clock
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {}

	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
	LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);

	// Let FREERTOS init the systick, we just leech off of it

	// Call global constructors
	int cpp_size = &(__preinit_array_end[0]) - &(__preinit_array_start[0]);
	for (int cpp_count = 0; cpp_count < cpp_size; ++cpp_count) {
		__preinit_array_start[cpp_count]();
	} 
	
	cpp_size = &(__init_array_end[0]) - &(__init_array_start[0]);
	for (int cpp_count = 0; cpp_count < cpp_size; ++cpp_count) {
		__init_array_start[cpp_count]();
	} 

	// Enable CACHE
	LL_FLASH_EnableInstCache();
	LL_FLASH_EnablePrefetch();

	// enable common peripherals, like DMA1 / SYSCFG
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
}
