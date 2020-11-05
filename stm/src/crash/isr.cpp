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
	if (LL_DMA_IsActiveFlag_TC5(DMA2)) {
		LL_DMA_ClearFlag_TC5(DMA2);
		crash::matrix.dma_finished();
	}
	if (LL_DMA_IsActiveFlag_TE5(DMA2)) {
		LL_DMA_ClearFlag_TE5(DMA2);
		crash::matrix.dma_finished();
	}
}

extern "C" void TIM1_BRK_TIM9_IRQHandler_Crash() {
	if (LL_TIM_IsActiveFlag_UPDATE(TIM9)) {
		LL_TIM_ClearFlag_UPDATE(TIM9);
		crash::matrix.timer_finished();
	}
}
