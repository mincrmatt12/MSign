#include "bootcmd.h"

// really just present here to check for STM and i couldn't be bothered to add a define
#if (defined(STM32F205xx) || defined(STM32F207xx)) && !defined(SIM)
#include "stm32f2xx_ll_rtc.h"
#include "stm32f2xx_ll_pwr.h"
#include "stm32f2xx_ll_rcc.h"
#include "stm32f2xx_ll_bus.h"

void bootcmd_init() {
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
	LL_RCC_EnableRTC(); // enable the RTC so we can play with it's goodies :)
	LL_PWR_EnableBkUpAccess();
	LL_RTC_DisableWriteProtection(RTC);
}

bool bootcmd_did_just_update() {
	bool x = LL_RTC_BAK_GetRegister(RTC, 1) != 0;
	LL_RTC_BAK_SetRegister(RTC, 1, 0x0);
	return x;
}

bootcmd_t bootcmd_get_cmd() {
	return (bootcmd_t)LL_RTC_BAK_GetRegister(RTC, 0);
}

uint32_t bootcmd_update_size() {
	return LL_RTC_BAK_GetRegister(RTC, 2);
}

void bootcmd_request_update(uint32_t size) {
	LL_RTC_BAK_SetRegister(RTC, 0, BOOTCMD_UPDATE);
	LL_RTC_BAK_SetRegister(RTC, 2, size);
}

void bootcmd_service_update() {
	LL_RTC_BAK_SetRegister(RTC, 0, BOOTCMD_RUN);
	LL_RTC_BAK_SetRegister(RTC, 1, 0xfece5);
}

void bootcmd_set_silent(bool on) {
	LL_RTC_BAK_SetRegister(RTC, 4, on);
}

bool bootcmd_get_silent() {
	return LL_RTC_BAK_GetRegister(RTC, 4) != 0;
}

const char * bootcmd_get_bl_revision() {
	return (const char *)&RTC->BKP3R;
}
#else
// mock interface
void bootcmd_init() {
}

bool bootcmd_did_just_update() {
	return false;
}

bootcmd_t bootcmd_get_cmd() {
	return 0;
}

uint32_t bootcmd_update_size() {
	return 0;
}

void bootcmd_request_update(uint32_t size) {
}

void bootcmd_service_update() {
}

const char * bootcmd_get_bl_revision() {
	return "fake";
}

static bool fakesilent = true;

bool bootcmd_get_silent() {
	return fakesilent;
}

void bootcmd_set_silent(bool c) {
	fakesilent = c;
}
#endif

