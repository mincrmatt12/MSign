#include "stm32f2xx_ll_rcc.h"
#include "stm32f2xx_ll_system.h"
#include "stm32f2xx_ll_utils.h"
#include "stm32f2xx_ll_cortex.h"
#include "stm32f2xx_ll_bus.h"
#include "rcc.h"

extern "C" {
	extern void (*__init_array_start [])(void);
	extern void (*__init_array_end [])(void);

	uint32_t SystemCoreClock;
	const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
	const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};
}

void rcc::init() {
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
	LL_RCC_HSI_Enable();

	/* Wait till HSI is ready */
	while(LL_RCC_HSI_IsReady() != 1)
	{

	}
	LL_RCC_HSI_SetCalibTrimming(16);
	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_13, 195, LL_RCC_PLLP_DIV_2);
	LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_13, 195, LL_RCC_PLLQ_DIV_5);
	LL_RCC_PLL_Enable();

	/* Wait till PLL is ready */
	while(LL_RCC_PLL_IsReady() != 1)
	{

	}
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

	/* Wait till System clock is ready */
	while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
	{

	}
	LL_SetSystemCoreClock(120000000);

	// Let FREERTOS init the systick, we just leech off of it

	// Initialize the cpp runtime

	int cpp_size = &(__init_array_end[0]) - &(__init_array_start[0]);
	for (int cpp_count = 0; cpp_count < cpp_size; ++cpp_count) {
		__init_array_start[cpp_count]();
	} 

	// Enable CACHE
	
	LL_FLASH_EnableInstCache();
	LL_FLASH_EnableDataCache();
	LL_FLASH_EnablePrefetch();
}
