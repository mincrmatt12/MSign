GPIOC->ODR = 0;
GPIOB->ODR = (1 << 6); // OE off

asm volatile ("nop");

// Clock out

for (int i = 0; i < 256; ++i) {
	if (i < 64) 
		GPIOC->ODR = (data[i] ? 7 : 0); // WHITE/BLACK
	else
		GPIOC->ODR = 0;
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	GPIOC->ODR |= (1 << 6);
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	GPIOC->ODR &= ~(1 << 6);
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
}
GPIOC->ODR = 0;

// LATCH
GPIOB->ODR |= (1 << 5);
asm volatile("nop");
asm volatile("nop");
GPIOB->ODR = 0; // LATCH off OE on
