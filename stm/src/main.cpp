#include "stm32f2xx.h"
#include "matrix.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "draw.h"
#include "fonts/freesans_14.h"
#include "fonts/dejavu_8.h"
#include "fonts/dejavu_10.h"
#include <stdlib.h>

led::Matrix<led::FrameBuffer<64, 32>> matrix;

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	//draw::text(matrix.get_inactive_buffer(), "test", font::freesans_14::metrics, font::freesans_14::data, 1, 16, 255, 0, 0);
	//draw::bitmap(matrix.get_inactive_buffer(), font::freesans_14::data[101], 7, 8, 1, 1, 1, 255, 0, 0);
	matrix.swap_buffers();
	char buf[20];
	int i = 0;
	while (true) {
		matrix.display();
		draw::fill(matrix.get_inactive_buffer(), 0, 0, 0);
		__itoa(i, buf, 10);
		int xs0 = draw::text(matrix.get_inactive_buffer(), buf, font::freesans_14::metrics, font::freesans_14::data, 1, 11, 255, 0, 0);
		int xs1 = draw::text(matrix.get_inactive_buffer(), "Brown foxes dogs", font::dejavusans_8::metrics, font::dejavusans_8::data, 1, 18, 0, 255, 0);
		int xs2 = draw::text(matrix.get_inactive_buffer(), buf, font::dejavusans_10::metrics, font::dejavusans_10::data, 1, 28, 0, 0, 255);
		
		draw::rect(matrix.get_inactive_buffer(), xs0, 11, 63, 12, 255, 127, 0);
		draw::rect(matrix.get_inactive_buffer(), xs1, 18, 63, 19, 127, 0, 255);
		draw::rect(matrix.get_inactive_buffer(), xs2, 28, 63, 29, 50, 50, 50);

		i += 1;
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
