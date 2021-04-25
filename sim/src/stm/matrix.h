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
						std::cout << "\x1b[48;2;" << std::to_string(_remap(_r(x, y))) << ";" << std::to_string(_remap(_g(x, y))) << ";" << std::to_string(_remap(_b(x, y))) << "m  "; 
					}                                                                                                                                                                 
					std::cout << std::endl;                                                                                                                                           
				}                                                                                                                                                                     
				portEXIT_CRITICAL();
			}                                                                                                                                                                         

			uint16_t & r(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _r(x, y);
				return junk;
			}
			uint16_t & g(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _g(x, y);
				return junk;
			}
			uint16_t & b(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _b(x, y);
				return junk;
			}

			const uint16_t & r(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _r(x, y);
				return junk;
			}
			const uint16_t & g(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _g(x, y);
				return junk;
			}
			const uint16_t & b(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _b(x, y);
				return junk;
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
			uint16_t data[Width*Height*3];

			inline uint16_t & _r(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 0];
			}

			inline uint16_t & _g(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 1];
			}

			inline uint16_t & _b(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 2];
			}

			inline uint8_t _remap(uint16_t prev) {                                    
				return std::pow((prev & 0xFFF) / 4095.0, (1/2.2)) * 255;
			}                                                                        


			uint16_t junk; // used as failsafe for read/write out of bounds
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
				xTaskCreate([](void *arg){((Matrix<FB> *)arg)->disptask();}, "dispint", 1024, this, 6, &me);
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
		private:                                                                         

			void disptask() {
				while (true) {
					_get_active_buffer().show();
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
