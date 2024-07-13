#ifdef USE_F2
#include <stm32f2xx.h>
#endif
#ifdef USE_F4
#include <stm32f4xx.h>
#endif
#include "common/bootcmd.h"
#include <stdbool.h>
#include "memset.h"

// BOOTLOADER REVISION CONSTANT
const char BL_REV[4] = {'2', 'c', 0, 0};

extern const void * _sirtext;
extern void * _srtext, *_ertext;
#define RAMFUNC __attribute__((section(".rtext")))

#define FLASH_KEY1 0x45670123U
#define FLASH_KEY2 0xCDEF89ABU

#ifdef STM32F205xx
// board
#define LED_DATA_PORT GPIOC
#define LED_DATA_BLOCK RCC_AHB1ENR_GPIOCEN
#else
// nucleo
#define LED_DATA_PORT GPIOD
#define LED_DATA_BLOCK RCC_AHB1ENR_GPIODEN
#endif

// BASIC SCREEN DRIVER
//
// Only controls one row of monochrome pixels
void init_scrn() {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | LED_DATA_BLOCK;
	asm volatile ("nop");
	asm volatile ("nop");
	asm volatile ("nop");

	GPIOB->MODER = (
		GPIO_MODER_MODE5_0 |
		GPIO_MODER_MODE6_0 |
		GPIO_MODER_MODE0_0 |
		GPIO_MODER_MODE1_0 |
		GPIO_MODER_MODE2_0 |
		GPIO_MODER_MODE3_0
	);

	LED_DATA_PORT->MODER = (
		GPIO_MODER_MODE6_0 |
		GPIO_MODER_MODE0_0 |
		GPIO_MODER_MODE1_0 |
		GPIO_MODER_MODE2_0 |
		GPIO_MODER_MODE3_0 |
		GPIO_MODER_MODE4_0 |
		GPIO_MODER_MODE5_0 
	);

	GPIOB->PUPDR = 0;
	GPIOB->ODR = (1 << 6);
	LED_DATA_PORT->ODR = 0;
}

void show_line(bool data[64]) {
#include "showline.inc"
}

RAMFUNC void show_line_ram(bool data[64]) {
#include "showline.inc"
}

void jump_to_program() {
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	SCB->VTOR = 0x4000;

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
	show_line_ram(sp);

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
		show_line_ram(sp); 
	}
	CLEAR_BIT(FLASH->CR, FLASH_CR_SER);

	// Sectors erased, program flash
	memset(sp, 0, 64);

	sp[0] = 1;

	if (size % 4 != 0) {
		size += (4 - (size % 4)); // round to next 4
	}

	int sp_pos = 2;
	int counter = 0;
	
	for (uint32_t i = 0; i < size; i += 4) {
		counter += 4;
		FLASH->CR |= FLASH_CR_PG;
		*(uint32_t *)(0x08004000 + i) = *(uint32_t *)(0x08080000 + i);

		while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {}

		if (counter > (size / 64)) {
			counter = 0;
			sp_pos += 1;
			if (sp_pos == 64) {
				memset(sp + 2, 0, 62);
				sp_pos = 2;
			}
			sp[sp_pos] = 1;
			show_line_ram(sp);
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
		show_line_ram(sp);
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
	bool sp[64] = {1, 1, 0, 0}; // 1, 0, 0, 0 - status RELOAD
	memset(sp + 4, 0, 60);

	show_line_ram(sp);

	if (bootcmd_update_size() > 16384) {
		sp[1] = 0;
		sp[3] = 1;
		sp[4] = 1;
		sp[5] = 1;
		sp[6] = 0;
		sp[7] = 0; // 1 1 0 0 - error in size of bootloader
		show_line_ram(sp);
		for (int i = 0; i < 30000; ++i) {
			asm volatile ("nop");
		}
		// UHOH
		NVIC_SystemReset(); // retry
	}

	volatile uint32_t update_size = bootcmd_update_size();

	// bootloader update loop
	// erase original bootloader
	//
	// NO FLASH FUNCTIONS CAN COME AFTER THIS
	
	CLEAR_BIT(FLASH->CR, FLASH_CR_PSIZE);
	FLASH->CR |= 2 << FLASH_CR_PSIZE_Pos;

	sp[4] = 1;
	sp[5] = 1; // 1, 1, 0, 0 - erasing sector
	show_line_ram(sp);
	
	CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
	FLASH->CR |= FLASH_CR_SER /* section erase + 0 sector */;
	FLASH->CR |= FLASH_CR_STRT; // WE are now gone

	while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {}
	CLEAR_BIT(FLASH->CR, FLASH_CR_SER);
	sp[5] = 0;
	sp[6] = 1;
	show_line_ram(sp); // erase OK, copying

	for (uint32_t ptr = 0x08000000, src = 0x08080000; ptr < (0x08000000 + update_size); ptr += 4, src += 4) {
		FLASH->CR |= FLASH_CR_PG;
		*((uint32_t *)ptr) = *((uint32_t *)src);
		while (FLASH->SR & FLASH_SR_BSY) {;}
	}

	sp[5] = 1;
	show_line_ram(sp); // 1, 1, 1 - OK
	FLASH->CR &= ~FLASH_CR_PG;

	// Set the BOOT MODE back to a normal value
	RTC->BKP0R = BOOTCMD_RUN;
	RTC->BKP1R = 0xd0d3;

	for (int i = 0; i < 30000; ++i) {asm volatile ("nop");}

	// Manually jump to the bootloader again
	
	__set_MSP(*(uint32_t *)(0x08000000));
	((void(*)(void))(*(uint32_t *)(0x08000004)))();

	// Did it return?
	sp[0] = 1;
	sp[1] = 0;
	sp[2] = 1;
	sp[3] = 0;
	sp[4] = 0;
	sp[5] = 0;
	sp[6] = 0;
	sp[7] = 0;
	sp[8] = 1; // ERROR 0, 0, 0, 0, 1 - new bootloader died
	show_line_ram(sp);

	while (1) {;}
}

int __attribute__((noreturn)) main() {
	bootcmd_init();
	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_3WS);

	if (bootcmd_get_cmd() == BOOTCMD_RUN) {
		// Set bootloader revision
		RTC->BKP3R = *(uint32_t *)(BL_REV);
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
	memcpy(&_srtext, &_sirtext, (uint32_t)(&_ertext) - (uint32_t)(&_srtext));

	if (bootcmd_get_cmd() == BOOTCMD_UPDATE) do_update_app();
	else do_update_bootloader();

	sp[1] = 0; // 1, 0, 1, 0 - error
	sp[4] = 0;
	sp[5] = 0;
	sp[6] = 1;
	sp[7] = 1;
	show_line(sp); // 0, 0, 1, 1 - returned from update handler
	
	while(1) {;} // spin forever
}
