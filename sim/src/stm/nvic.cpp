#include "nvic.h"                                             
                                                              
#include "draw.h"
#include "srv.h"                                              
#include "tasks/timekeeper.h"                                 
#include "matrix.h"                                           
#include "fonts/lcdpixel_6.h"
                                                              
extern matrix_type matrix;          
extern srv::Servicer servicer;                                
extern tasks::Timekeeper timekeeper;                          
                                                              
void nvic::init() {                                           
    // TODO                                                   
}                                                             
                                                              
extern "C" void DMA2_Stream5_IRQHandler() {                   
}                                                             
                                                              
extern "C" void TIM1_BRK_TIM9_IRQHandler() {                  
}                                                             
                                                              
extern "C" void DMA2_Stream2_IRQHandler() {                   
}                                                             
                                                              
extern "C" void DMA2_Stream7_IRQHandler() {                   
}                                                             
                                                              
extern "C" void SysTick_Handler() {                           
}                                                             

extern "C" void vApplicationTickHook() {
	timekeeper.systick_handler();
}

extern void NVIC_SystemReset();

namespace {
	void draw_hardfault_screen(int remain_loop) {
		draw::fill(matrix.get_inactive_buffer(), 0, 0, 0);
		draw::text(matrix.get_inactive_buffer(), "MSign crashed!", font::lcdpixel_6::info, 0, 6, 4095, 1, 1);
		draw::text(matrix.get_inactive_buffer(), "rebooting in:", font::lcdpixel_6::info, 0, 12, 128_c, 128_c, 128_c);

		draw::rect(matrix.get_inactive_buffer(), 0, 24, remain_loop / 2, 32, 100_c, 100_c, 4095);
	}
}

[[noreturn]] void nvic::show_error_screen(const char * errcode) {
	// there was a hardfault... delay for a while so i know
	for (int j = 0; j < 128; ++j) {
		draw_hardfault_screen(j);
		draw::text(matrix.get_inactive_buffer(), errcode, font::lcdpixel_6::info, 0, 20, 4095, 128_c, 0);
		matrix.swap_buffers_from_isr();
	}

	NVIC_SystemReset();
	while(1) {;}
}
