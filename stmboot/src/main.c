#include "stm32f2xx.h"

int main() {
	// BASIC BOOTLOADER -- set VTOR and jump.
	
	SCB->VTOR = 0x4000;
	((void (*)())(0x80004000))();
}
