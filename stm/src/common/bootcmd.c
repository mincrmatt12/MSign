#include "bootcmd.h"
#include "stm32f2xx_ll_rtc.h"
#include "stm32f2xx_ll_pwr.h"

void bootcmd_init() {
	LL_PWR_EnableBkUpAccess();
	LL_RTC_DisableWriteProtection(RTC);
}

bool bootcmd_did_just_update() {
	return LL_RTC_BAK_GetRegister(RTC, 1) != 0;
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
	LL_RTC_BAK_SetRegister(RTC, 1, 0xfece);
}
