#include "rng.h"                 
#include <random>                
                                 
std::default_random_engine rndy; 
                                 
void rng::init() {               
}                                
                                 
uint32_t rng::get() {            
    return (uint32_t)rndy();     
}                                

uint16_t rng::getclr() {
	return std::pow((rng::get() % 4096) / 4095.0, 2.2) * 4095.0;
}
