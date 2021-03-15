/* extremely simplified startup.s 
 * 
 * since the bootloader doesn't use bss / data (only stack local stuff)
 * we don't even need any startup code; we just point the Reset_Handler
 * at main. We also don't provide a full ISR table as no NVIC exceptions
 * are ever turned on
 */

.syntax unified
.cpu cortex-m3
.thumb

.global g_pfnVectors
.global fault_handler
.global main

.section .text,"ax",%progbits

fault_handler:
	b fault_handler

.size fault_handler, .-fault_handler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object

g_pfnVectors:
	.word _estack
	.word main

	.word  fault_handler
	.word  fault_handler
	.word  fault_handler
	.word  fault_handler
	.word  fault_handler
	.word  0
	.word  0
	.word  0
	.word  0
	.word  fault_handler
	.word  fault_handler
	.word  0
	.word  fault_handler
	.word  fault_handler

.size  g_pfnVectors, .-g_pfnVectors
