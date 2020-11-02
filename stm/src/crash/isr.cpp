#include <stdint.h>
#include <stm32f205xx.h>
#include "simplematrix.h"

namespace crash {
	extern Matrix matrix;
}

extern "C" void HardFault_Handler_Crash() {
	// just reset the system
	NVIC_SystemReset();
}

extern "C" void DMA2_Stream5_IRQHandler_Crash() {
	//crash::matrix.dma_finish();
}

extern "C" void TIM1_BRK_TIM9_IRQHandler_Crash() {
	//crash::matrix.tim_elapsed();
}
