#include "stm32f2xx.h"
#include "matrix.h"
#include "nvic.h"
#include "rcc.h"

led::Matrix<led::FrameBuffer<64, 32>> matrix;

int main() {
	rcc::init();
	nvic::init();

	matrix.init();
	for (int i = 0; i < 64; ++i) {
		for (int j = 0; j < 32; ++j) {
			matrix.get_inactive_buffer().r(i, j) = 0;
			matrix.get_inactive_buffer().g(i, j) = 0;
			matrix.get_inactive_buffer().b(i, j) = 255;
		}
	}
	matrix.swap_buffers();
	int i = 0, j = 0, k = 0, y = 0;
	while (true) {
		matrix.display();
		i += 1; y += 1;
		y %= 30;
		if (i == 256) {i = 0; ++j;}
		if (j == 256) {j = 0; ++k;}
		if (k == 256) {k = 0;}
		for (int x = 0; x < 64; ++ x) {
			matrix.get_inactive_buffer().r(x, y) = i;
			matrix.get_inactive_buffer().g(x, y) = j;
			matrix.get_inactive_buffer().b(x, y) = k;
			matrix.get_inactive_buffer().r(x, y+1) = i;
			matrix.get_inactive_buffer().g(x, y+1) = j;
			matrix.get_inactive_buffer().b(x, y+1) = k;
		}
		while (matrix.is_active()) {;}
		matrix.swap_buffers();
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
