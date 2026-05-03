#include "ucpd.h"

#include <stm32g0xx_ll_ucpd.h>
#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_system.h>

namespace ucpd {
	void strobe_preinit() {
		LL_UCPD_InitTypeDef init {
			.psc_ucpdclk = LL_UCPD_PSC_DIV2, // ucpd clk is 16MHz HSI, div / 2 = 8MHz (which is in recommended 6-8mhz range)
			.transwin = 7, // divide by 8   - stolen from the LL struct init
			.IfrGap = 16,  // divide by 17
			.HbitClockDiv = 13, // divide by 14, just under 600khz
		};

		LL_UCPD_Init(UCPD1, &init);
		LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
		
		LL_UCPD_Enable(UCPD1);
		LL_UCPD_SetSNKRole(UCPD1);
		LL_UCPD_SetccEnable(UCPD1, LL_UCPD_CCENABLE_CC1CC2);
		LL_UCPD_TypeCDetectionCC1Enable(UCPD1);
		LL_UCPD_TypeCDetectionCC1Enable(UCPD2);

		SYSCFG->CFGR1 |= SYSCFG_CFGR1_UCPD1_STROBE;

		LL_SYSCFG_EnablePinRemap(LL_SYSCFG_PIN_RMP_PA11 | LL_SYSCFG_PIN_RMP_PA12);
	}
}
