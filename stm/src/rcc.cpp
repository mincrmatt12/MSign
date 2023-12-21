#include "rcc.h"

#ifdef USE_F2
#include <stm32f2xx_ll_rcc.h>
#include <stm32f2xx_ll_system.h>
#include <stm32f2xx_ll_utils.h>
#include <stm32f2xx_ll_cortex.h>
#include <stm32f2xx_ll_bus.h>

#define FL_LATENCY LL_FLASH_LATENCY_3
#endif

#ifdef USE_F4
#include <stm32f4xx_ll_rcc.h>
#include <stm32f4xx_ll_system.h>
#include <stm32f4xx_ll_utils.h>
#include <stm32f4xx_ll_cortex.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_pwr.h>

#define FL_LATENCY LL_FLASH_LATENCY_5
#endif

#include <reent.h>

extern "C" {
	extern void (*__preinit_array_start [])(void);
	extern void (*__preinit_array_end [])(void);
	extern void (*__init_array_start [])(void);
	extern void (*__init_array_end [])(void);

	uint32_t SystemCoreClock;
	const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
	const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};
}

void rcc::init() {
	LL_FLASH_SetLatency(FL_LATENCY);
	while (LL_FLASH_GetLatency() != FL_LATENCY) {}

#ifdef USE_F4
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
#endif

	// Turn on the HSI oscillator and reset its trimming
	LL_RCC_HSI_SetCalibTrimming(16);
	LL_RCC_HSI_Enable();

	// Wait till HSI is ready
	while (LL_RCC_HSI_IsReady() != 1) {}

#if defined(USE_F2) // 120mhz
	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_13, 195, LL_RCC_PLLP_DIV_2);
	LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_13, 195, LL_RCC_PLLQ_DIV_5);
#elif defined(USE_F4) // 168 mhz
	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 168, LL_RCC_PLLP_DIV_2);
	LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 168, LL_RCC_PLLQ_DIV_7);
#endif
	LL_RCC_PLL_Enable();

	// Wait for PLL to start
	while (LL_RCC_PLL_IsReady() != 1) {}

	// Setup ahb/apb prescaler and switch to PLL for system clock
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

	// Wait for clock to be switched
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {}

	LL_SetSystemCoreClock(F_CPU);

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
	LL_FLASH_EnableDataCache();
	LL_FLASH_EnablePrefetch();
}
