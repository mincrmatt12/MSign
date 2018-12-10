#include "stm32f2xx.h"
#include "matrix.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "draw.h"
#include "fonts/latob_15.h"
#include <stdlib.h>
#include <cmath>

led::Matrix<led::FrameBuffer<64, 32>> matrix;

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	//draw::text(matrix.get_inactive_buffer(), "test", font::freesans_14::metrics, font::freesans_14::data, 1, 16, 255, 0, 0);
	//draw::bitmap(matrix.get_inactive_buffer(), font::freesans_14::data[101], 7, 8, 1, 1, 1, 255, 0, 0);
	matrix.swap_buffers();

	uint16_t x = 0;
	char buf[5];
	while (true) {
		matrix.display();
		if ((rng::get() % 128) < 110) {;}
		else {x += rng::get() % 3;}
		x %= 255;
		for (uint16_t x0 = 0; x0 < 64; ++x0) {
			uint16_t mult = abs(x0-(x-95));
			draw::rect(matrix.get_inactive_buffer(), x0, 0, x0+1, 32, mult, mult, mult);
		}
		__itoa(x, buf, 10);
		draw::text(matrix.get_inactive_buffer(), buf, font::lato_bold_15::info, 0, 16, 255, 127, 0);
		while (matrix.is_active()) {
		}
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
