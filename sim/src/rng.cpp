#include "rng.h"
#include <random>

std::default_random_engine rndy;

void rng::init() {
}

uint32_t rng::get() {
	return (uint32_t)rndy();
}
