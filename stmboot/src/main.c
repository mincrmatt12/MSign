#include "stm32f2xx.h"
#include "common/bootcmd.h"
#include "stm32f2xx_ll_system.h"
#include <stdbool.h>
#include "string.h"

void jump_to_program();

int main() {
	bootcmd_init();
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);

	if (bootcmd_get_cmd() == BOOTCMD_RUN) {
		jump_to_program();

		while (1) {;}
	}
	if (bootcmd_get_cmd() != BOOTCMD_UPDATE) {
		while (1) {;}
	}

	// We are updating, unlock flash
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;

	uint8_t sector_count = 0;

	// Erase sectors
	uint32_t size = bootcmd_update_size();
	if (size > 0) sector_count++;
	if (size > 16384) sector_count++;
	if (size > 32768) sector_count++;
	if (size > 49152) sector_count++;
	if (size > 114688) sector_count++;
	if (size > 245760) sector_count++;
	if (size > 376832) sector_count++;

	// set PSIZE
	CLEAR_BIT(FLASH->CR, FLASH_CR_PSIZE);
	FLASH->CR |= 2 << FLASH_CR_PSIZE_Pos;
	
	for (uint8_t i = 0; i < sector_count; ++i) {
		CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
		FLASH->CR |= FLASH_CR_SER /* section erase */ | ((1 + i) << FLASH_CR_SNB_Pos);
		FLASH->CR |= FLASH_CR_STRT;

		while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {}
	}
	CLEAR_BIT(FLASH->CR, FLASH_CR_SER);

	// Sectors erased, program flash


	if (size % 4 != 0) {
		size += (4 - (size % 4)); // round to next 4
	}
	
	for (uint32_t i = 0; i < size; i += 4) {
		FLASH->CR |= FLASH_CR_PG;
		*(uint32_t *)(0x08004000 + i) = *(uint32_t *)(0x08080000 + i);

		while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {}
	}

	FLASH->CR |= FLASH_CR_LOCK;

	if (memcmp((const void *)(0x08004000), (const void *)(0x08080000), bootcmd_update_size()) != 0) {
		// UHOH
		NVIC_SystemReset();
	}

	// verify is ok, we can now erase the update partition and mark everything as hunkidory.
	// this also means that a program can check a 
	
	bootcmd_service_update();

	// the system is now updated, reset
	NVIC_SystemReset();
}

void jump_to_program() {

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	__set_MSP(*(uint32_t *)(0x08004000));

	((void(*)(void))(*(uint32_t *)(0x08004004)))();
}
