#include "rng.h"

#include "draw.h"
#include "stm32f2xx.h"
#include "stm32f2xx_ll_rng.h"
#include "stm32f2xx_ll_bus.h"

#include <cmath>

void rng::init() {
	LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_RNG);

	LL_RNG_Enable(RNG);
}

uint32_t rng::get() {
	while (!LL_RNG_IsActiveFlag_DRDY(RNG)) {;}

	if (LL_RNG_IsActiveFlag_CECS(RNG) || LL_RNG_IsActiveFlag_SECS(RNG)) {
		LL_RNG_Disable(RNG);
		LL_RNG_Enable(RNG);
		return 0x4158f;  // chosen by dice roll certainly random...
	}

	return LL_RNG_ReadRandData32(RNG);
}

uint16_t rng::getclr() {
	return draw::cvt(get() % 256);
}
