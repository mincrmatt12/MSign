#include "stm32f205xx.h"
#include "stm32f2xx.h"
#include "common/bootcmd.h"
#include <stdbool.h>
#include "string.h"

extern const void * _sirtext;
extern void * _srtext, *_ertext;
#define RAMFUNC __attribute__((section(".rtext")))

// BASIC SCREEN DRIVER
//
// Only controls one row of monochrome pixels
void init_scrn() {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
	asm volatile ("nop");
	asm volatile ("nop");
	asm volatile ("nop");

	GPIOB->MODER = (
		GPIO_MODER_MODE5_0 |
		GPIO_MODER_MODE6_0
	);

	GPIOC->MODER = (
		GPIO_MODER_MODE6_0 |
		GPIO_MODER_MODE0_0 |
		GPIO_MODER_MODE1_0 |
		GPIO_MODER_MODE2_0 
	);

	GPIOB->PUPDR = 0;
}

void show_line(bool data[64]) {
	GPIOC->ODR = 0;
	GPIOB->ODR = (1 << 6); // OE off

	asm volatile ("nop");

	// Clock out
	
	for (int i = 0; i < 64; ++i) {
		GPIOC->ODR &= ~(1<<6); // CLK OFF
		GPIOC->ODR = (data[i] | data[i] << 1 | data[i] << 2); // WHITE/BLACK
		asm volatile("nop");
		GPIOC->ODR |= (1 << 6);
		asm volatile("nop");
		asm volatile("nop");
	}

	GPIOC->ODR &= ~(1<<6); // CLK OFF
	
	// LATCH
	
	GPIOC->ODR |= (1 << 5);
	asm volatile("nop");
	asm volatile("nop");
	GPIOC->ODR = 0; // LATCH off OE on
}

void jump_to_program() {

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	__set_MSP(*(uint32_t *)(0x08004000));

	((void(*)(void))(*(uint32_t *)(0x08004004)))();
}

RAMFUNC void do_update_app() {
	uint8_t sector_count = 0;
	bool sp[64] = {1, 0, 0, 0}; // 1, 0, 0, 0 - status UPDATE
	memset(sp + 4, 0, 60);

	// Erase sectors
	uint32_t size = bootcmd_update_size();
	if (size > 0) {sector_count++; sp[4] = 1;}
	if (size > 16384) {sector_count++; sp[5] = 1;}
	if (size > 32768) {sector_count++; sp[6] = 1;}
	if (size > 49152) {sector_count++; sp[7] = 1;}
	if (size > 114688) {sector_count++; sp[8] = 1;}
	if (size > 245760) {sector_count++; sp[9] = 1;}
	if (size > 376832) {sector_count++; sp[10] = 1;}

	sp[12] = 0;
	sp[13] = 1;
	sp[14] = 1;
	sp[15] = 0; // status code 0, 1, 1, 0, leds indicate erase count
	show_line(sp);

	// set PSIZE
	CLEAR_BIT(FLASH->CR, FLASH_CR_PSIZE);
	FLASH->CR |= 2 << FLASH_CR_PSIZE_Pos;

	sp[13] = 0; // status code 0, 0, 1, 0, leds indicate erase remaining
	
	for (uint8_t i = 0; i < sector_count; ++i) {
		CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
		FLASH->CR |= FLASH_CR_SER /* section erase */ | ((1 + i) << FLASH_CR_SNB_Pos);
		FLASH->CR |= FLASH_CR_STRT;

		while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {}

		sp[i+4] = 0;
		show_line(sp); 
	}
	CLEAR_BIT(FLASH->CR, FLASH_CR_SER);

	// Sectors erased, program flash
	memset(sp, 0, 64);

	sp[0] = 1;

	if (size % 4 != 0) {
		size += (4 - (size % 4)); // round to next 4
	}

	int sp_pos = 2;
	
	for (uint32_t i = 0; i < size; i += 4) {
		FLASH->CR |= FLASH_CR_PG;
		*(uint32_t *)(0x08004000 + i) = *(uint32_t *)(0x08080000 + i);

		while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {}

		if (i % (size >> 3)) {
			sp_pos += 1;
			if (sp_pos == 64) {
				memset(sp + 2, 0, 62);
				sp_pos = 2;
			}
			sp[sp_pos] = 1;
			show_line(sp);
		}
	}

	FLASH->CR |= FLASH_CR_LOCK;

	memset(sp + 1, 0, 63);

	sp[4] = 1;
	sp[5] = 0;
	sp[6] = 1;
	sp[7] = 1; // status 1 0 1 1  - comparing

	if (memcmp((const void *)(0x08004000), (const void *)(0x08080000), bootcmd_update_size()) != 0) {
		sp[0] = 1;
		sp[3] = 1;
		sp[4] = 1;
		sp[5] = 1;
		sp[6] = 1;
		sp[7] = 1; // ERROR - compare fail
		for (int i = 0; i < 30000; ++i) {
			asm volatile ("nop");
		}
		// UHOH
		NVIC_SystemReset(); // retry
	}

	// verify is ok, we can now erase the update partition and mark everything as hunkidory.
	// this also means that a program can check a 
	
	bootcmd_service_update();

	// the system is now updated, reset
	NVIC_SystemReset();
}

RAMFUNC void do_update_bootloader() {
}

int main() {
	bootcmd_init();
	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_3WS);

	if (bootcmd_get_cmd() == BOOTCMD_RUN) {
		jump_to_program();

		while (1) {;}
	}

	// Enable screen
	init_scrn();

	if (bootcmd_get_cmd() != BOOTCMD_UPDATE && bootcmd_get_cmd() != BOOTCMD_NEW_LOADER) {
		// Show test pattern
		bool tp[64] = {1, 0, 1, 0, 0, 1, 1, 0}; // 1, 0, 1, 0 - error
												// 0, 1, 1, 0 - invalid command
		show_line(tp);
		while (1) {;}
	}

	bool sp[64] = {1, 1, 1, 0, 0, 1, 1, 1}; // 1, 1, 1, 0 - status
											// 0, 1, 1, 1 - unlocking flash
	show_line(sp);

	// We are updating, unlock flash
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;

	sp[4] = 1;
	sp[7] = 0; // 1, 1, 1, 0 - copying code
	show_line(sp);

	// Load RAMFUNCs
	memcpy(&_srtext, &_sirtext, &_ertext - &_srtext);

	if (bootcmd_get_cmd() == BOOTCMD_UPDATE) do_update_app();
	else do_update_bootloader();

	sp[1] = 0; // 1, 0, 1, 0 - error
	sp[4] = 0;
	sp[5] = 0;
	sp[6] = 1;
	sp[7] = 1;
	show_line(sp); // 0, 0, 1, 1 - returned from update handler
}
