#ifndef MATRIX_H
#define MATRIX_H

#include "gpio.h"
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <chrono>
#include <cmath>

namespace led {

	template<uint16_t Width, uint16_t Height>
		struct FrameBuffer {
			FrameBuffer() {
				memset(data, 0x0, sizeof(data));
			}

			void show() {                                                                                                                                                             
				std::cout << "\e[;H\e[?25l";                                                                                                                                                 
				for (size_t y = 0; y < Height; ++y) {                                                                                                                                 
					for (size_t x = 0; x < Width; ++x) {                                                                                                                              
						std::cout << "\x1b[48;2;" << std::to_string(_remap(_r(x, y))) << ";" << std::to_string(_remap(_g(x, y))) << ";" << std::to_string(_remap(_b(x, y))) << "m  "; 
					}                                                                                                                                                                 
					std::cout << std::endl;                                                                                                                                           
				}                                                                                                                                                                     
			}                                                                                                                                                                         

			uint8_t & r(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _r(x, y);
				return junk;
			}
			uint8_t & g(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _g(x, y);
				return junk;
			}
			uint8_t & b(uint16_t x, uint16_t y) {
				if (on_screen(x, y))
					return _b(x, y);
				return junk;
			}

			const uint8_t & r(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _r(x, y);
				return junk;
			}
			const uint8_t & g(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _g(x, y);
				return junk;
			}
			const uint8_t & b(uint16_t x, uint16_t y) const {
				if (on_screen(x, y))
					return _b(x, y);
				return junk;
			}

			inline bool on_screen(uint16_t x, uint16_t y) const {
				return (x < Width && y < Height);
			}

			uint8_t * byte_stream;

			static constexpr uint16_t width = Width;
			static constexpr uint16_t height = Height;
			static constexpr uint16_t stb_lines = Height / 2;

			private:
			uint8_t data[Width*Height*3];

			inline uint8_t & _r(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 0];
			}

			inline uint8_t & _g(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 1];
			}

			inline uint8_t & _b(uint16_t x, uint16_t y) {
				return data[x*3 + y*Width*3 + 2];
			}

			inline uint8_t _remap(uint8_t prev) {                                    
				if (prev == 0) return 0;                                             
				float inter = (float)prev / 255.0;                                   
				return std::min(255, (uint8_t)(255.0 * std::pow(inter, 0.5)) + 10);  
			}                                                                        


			uint8_t junk; // used as failsafe for read/write out of bounds
		};

	template<typename FB>                                                                
		struct Matrix {                                                                  

			Matrix() : fb0(), fb1() {                                                    
			}                                                                            
			void init() {                                                                
			}                                                                            

			void display() {                                                             
				using namespace std::chrono;

				this->_get_active_buffer().show();                                       
				started_at = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
			}                                                                            

			void swap_buffers() {                                                        
				if (is_active()) return;                                                 
				active_buffer = !active_buffer;                                          
			}                                                                            

			FB& get_inactive_buffer() {return active_buffer ? fb1 : fb0;}                
			const FB& get_active_buffer() {return active_buffer ? fb0 : fb1;}            
			volatile bool is_active() {                                                  
				using namespace std::chrono;

				return (duration_cast<milliseconds>(system_clock::now().time_since_epoch()) - started_at) < (1000ms / 30);                                                            
			}                                                                            

		private:                                                                         
			FB fb0, fb1;                                                                 
			bool active_buffer = false;                                                  
			uint8_t pos = 0;                                                             
			uint16_t row = 0;                                                            
			uint8_t dma_buffer[(FB::width * 2) + 1];                                     

			std::chrono::milliseconds started_at;

			// impl for the wait system                                                  
			uint16_t delay_counter = 0;                                                  
			bool delaying = false;                                                       

			// show variable                                                             
			bool show = false;                                                           

			// clock is tied to tim2_ch3                                                 

			FB& _get_active_buffer() {return active_buffer ? fb0 : fb1;}                 
		};                                                                               
}

#endif
