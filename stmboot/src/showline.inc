LED_DATA_PORT->ODR = 0;
GPIOB->ODR = (1 << 6); // OE off

asm volatile ("nop");

// Clock out

for (int i = 0; i < 256; ++i) {
	if (i >= 192) 
		LED_DATA_PORT->ODR = (data[255 - i] ? 7 : 0); // WHITE/BLACK
	else
		LED_DATA_PORT->ODR = 0;
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	LED_DATA_PORT->ODR |= (1 << 6);
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	LED_DATA_PORT->ODR &= ~(1 << 6);
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
}
LED_DATA_PORT->ODR = 0;

// LATCH
GPIOB->ODR |= (1 << 5);
asm volatile("nop");
asm volatile("nop");
GPIOB->ODR = 0; // LATCH off OE on
