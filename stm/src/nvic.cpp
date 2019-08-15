#include "nvic.h"
#include "pins.h"

#include "stm32f2xx.h"
#include "stm32f2xx_hal.h"
#include "stm32f2xx_hal_tim.h"

#include "srv.h"
#include "tasks/timekeeper.h"
#include "matrix.h"


extern led::Matrix<led::FrameBuffer<64, 32>> matrix;
extern srv::Servicer servicer;
extern tasks::Timekeeper timekeeper;

void nvic::init() {
	SCB->VTOR = 0x4000; // set vector table relocation to 0x4000 since that's where our image starts.
	NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

	NVIC_SetPriority(DMA2_Stream5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),6, 0));
	NVIC_EnableIRQ(DMA2_Stream5_IRQn);

	NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
	NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);

	NVIC_SetPriority(NVIC_SRV_TX_IRQ_NAME, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),4, 0));
	NVIC_EnableIRQ(NVIC_SRV_TX_IRQ_NAME);

	NVIC_SetPriority(NVIC_SRV_RX_IRQ_NAME, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),6, 0));
	NVIC_EnableIRQ(NVIC_SRV_RX_IRQ_NAME);

	NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),2, 0));
	NVIC_EnableIRQ(SysTick_IRQn);
}

extern "C" void DMA2_Stream5_IRQHandler() {
	if (LL_DMA_IsActiveFlag_TC5(DMA2)) {
		LL_DMA_ClearFlag_TC5(DMA2);
		matrix.dma_finish();
	}
}

extern "C" void TIM1_BRK_TIM9_IRQHandler() {
	if (LL_TIM_IsActiveFlag_UPDATE(TIM9)) {
		LL_TIM_ClearFlag_UPDATE(TIM9);
		matrix.tim_elapsed();
	}
}

extern "C" void NVIC_SRV_RX_IRQ_HANDLER() {
	if (NVIC_SRV_RX_ACTV(UART_DMA)) {
		NVIC_SRV_RX_CLRF(UART_DMA);
		servicer.dma_finish(true);
	}
}

extern "C" void NVIC_SRV_TX_IRQ_HANDLER() {
	if (NVIC_SRV_TX_ACTV(UART_DMA)) {
		NVIC_SRV_TX_CLRF(UART_DMA);
		servicer.dma_finish(false);
	}
}

extern "C" void SysTick_Handler() {
	timekeeper.systick_handler();
}

extern "C" void HardFault_Handler() {
	// there was a hardfault... delay for a while so i know
	for (int i = 0; i < F_CPU; ++i) {
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("nop"); // wait 3 seconds
	}
}
