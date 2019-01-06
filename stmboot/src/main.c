#include "stm32f2xx.h"
#include "stm32f2xx_ll_rcc.h"

int main() {
	// BASIC BOOTLOADER -- set VTOR and jump.
	
	LL_RCC_DeInit();

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	__set_MSP(*(uint32_t *)(0x08004000));

	uint32_t address = *(uint32_t *)(0x08004004);
	( (void ( * )( void ) ) address ) ();
}
