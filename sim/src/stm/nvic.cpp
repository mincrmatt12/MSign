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

void nvic::setup_isrs_for_flash(bool enable) {
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
