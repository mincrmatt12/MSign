#ifndef MATRIX_H
#define MATRIX_H

#include "gpio.h"
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <FreeRTOS.h>
#include <task.h>

#include "tskmem.h"
#include "color.h"

namespace led {

	template<uint16_t Width, uint16_t Height>
		struct FrameBuffer {
			FrameBuffer() {
				memset(data, 0x0, sizeof(data));
			}

			void clear() {
				memset(data, 0x0, sizeof(data));
			}

			void show() {                                                                                                                                                             
				portENTER_CRITICAL();
				std::cout << "\e[;H\e[?25l";                                                                                                                                                 
				for (size_t y = 0; y < Height; ++y) {                                                                                                                                 
					for (size_t x = 0; x < Width; ++x) {                                                                                                                              
						std::cout << "\x1b[48;2;" << std::to_string(_remap(_at(x, y).r)) << ";" << std::to_string(_remap(_at(x, y).g)) << ";" << std::to_string(_remap(_at(x, y).b)) << "m  "; 
					}                                                                                                                                                                 
					std::cout << std::endl;                                                                                                                                           
				}                                                                                                                                                                     
				portEXIT_CRITICAL();
			}                                                                                                                                                                         

			color_t & at(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _at(x, y);
				return junk;
			}

			const color_t & at(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _at(x, y);
				return junk;
			}

			color_t & at_unsafe(uint16_t x, uint16_t y) {
				return _at(x, y);
			}

			const color_t & at_unsafe(uint16_t x, uint16_t y) const {
				return _at(x, y);
			}

			inline bool on_screen(uint16_t x, uint16_t y) const {
				return (x < Width && y < Height);
			}

			uint8_t * byte_stream;

			static constexpr uint16_t width = Width;
			static constexpr uint16_t effective_width = Width;
			static constexpr uint16_t height = Height;
			static constexpr uint16_t stb_lines = Height / 2;

		private:
			color_t data[Width*Height*3];

			inline color_t & _at(uint16_t x, uint16_t y) {
				return data[x + y*Width];
			}

			inline const color_t & _at(uint16_t x, uint16_t y) const {
				return data[x + y*Width];
			}

			inline uint8_t _remap(uint16_t prev) {                                    
				return std::pow((prev & 0xFFF) / 4095.0, (1/2.6)) * 255;
			}                                                                        


			color_t junk; // used as failsafe for read/write out of bounds
		};

	template<typename FB>                                                                
		struct Matrix {                                                                  
			typedef FB framebuffer_type;

			Matrix() : fb0(), fb1() {                                                    
			}                                                                            

			void init() {                                                                
			}                                                                            

			void start_display() {                                                             
				should_swap = false;
				me = dispinttask.create([](void *arg){((Matrix<FB> *)arg)->disptask();}, "dispint", this, 6);
			}

			// This should only be called from an RTOS task
			void swap_buffers() {
				//active_buffer = !active_buffer;
				notify_when_swapped = xTaskGetCurrentTaskHandle();
				should_swap = true;

				// This uses the task notification value to wait for something to happen.
				// This _does_ preclude the use of notification values for anything else.
				xTaskNotifyWait(0, 0xffffffffUL, NULL, portMAX_DELAY);
			}

			void swap_buffers_from_isr() {
				swap_buffers();
			}

			FB& get_inactive_buffer() {return active_buffer ? fb1 : fb0;}                
			const FB& get_active_buffer() {return active_buffer ? fb0 : fb1;}            

			volatile bool force_off = false;
		private:                                                                         
			tskmem::TaskHolder<1024> dispinttask;

			void disptask() {
				while (true) {
					if (!force_off) _get_active_buffer().show();
					vTaskDelay(pdMS_TO_TICKS(1000/60));
					// We have finished clocking out a row, process buffer swaps
					if (should_swap) {
						active_buffer = !active_buffer;
						if (notify_when_swapped) xTaskNotifyFromISR(notify_when_swapped, 1, eSetValueWithOverwrite, NULL);
					}
				}
			}

			FB fb0, fb1;                                                                 
			bool active_buffer = false;                                                  
			uint8_t pos = 0;                                                             
			uint16_t row = 0;                                                            
			uint8_t dma_buffer[(FB::width * 2) + 1];                                     

			// impl for the wait system                                                  
			uint16_t delay_counter = 0;                                                  
			bool delaying = false;                                                       

			// show variable                                                             
			bool show = false;                                                           

			// clock is tied to tim2_ch3                                                 

			FB& _get_active_buffer() {return active_buffer ? fb0 : fb1;}                 

			TaskHandle_t me, notify_when_swapped = nullptr;
			bool should_swap = false;
		};                                                                               
}

#endif
