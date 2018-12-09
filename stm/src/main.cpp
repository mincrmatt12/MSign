#include "stm32f2xx.h"
#include "matrix.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"

led::Matrix<led::FrameBuffer<64, 32>> matrix;

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	int b = 0;
	for (int x = 0; x < 64; ++x) {
		for (int y = 0; y < 32; ++y) {
			matrix.get_inactive_buffer().r(x, y) = rng::get() % 256;
			matrix.get_inactive_buffer().g(x, y) = rng::get() % 256;
			matrix.get_inactive_buffer().b(x, y) = rng::get() % 256;
		}
	}
	matrix.swap_buffers();
	while (true) {
		matrix.display();
		while (matrix.is_active()) {
		}
	}
}

extern "C" void DMA2_Stream5_IRQHandler() {
	if (LL_DMA_IsActiveFlag_TC5(DMA2)) {
		LL_DMA_ClearFlag_TC5(DMA2);
		matrix.dma_finish();
	}
}

extern "C" void TIM1_UP_TIM10_IRQHandler() {
	if (LL_TIM_IsActiveFlag_UPDATE(TIM1)) {
		LL_TIM_ClearFlag_UPDATE(TIM1);
		matrix.tim_elapsed();
	}
}
