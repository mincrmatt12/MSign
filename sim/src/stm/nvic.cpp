#include "nvic.h"                                             
                                                              
#include "draw.h"
#include "srv.h"                                              
#include "tasks/timekeeper.h"                                 
#include "matrix.h"                                           
                                                              
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
                                                              

