#include "stm32f2xx.h"
#include "matrix.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "draw.h"
#include "fonts/freesans_14.h"
#include "fonts/comic_14.h"
#include "fonts/lato_12.h"
#include <stdlib.h>

led::Matrix<led::FrameBuffer<64, 32>> matrix;

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	//draw::text(matrix.get_inactive_buffer(), "test", font::freesans_14::metrics, font::freesans_14::data, 1, 16, 255, 0, 0);
	//draw::bitmap(matrix.get_inactive_buffer(), font::freesans_14::data[101], 7, 8, 1, 1, 1, 255, 0, 0);
	int i = 0;
	draw::fill(matrix.get_inactive_buffer(), 0, 0, 0);
	draw::text(matrix.get_inactive_buffer(), "Comic Sans!", font::comic_14::info, 1, 15, 255, 255, 255);
	draw::text(matrix.get_inactive_buffer(), "Lato!", font::lato_regular_12::info, 1, 31, 255, 255, 255);
	matrix.swap_buffers();
	while (true) {
		matrix.display();
		i += 1;
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
