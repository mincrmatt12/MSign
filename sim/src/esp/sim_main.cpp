#include "esp_log.h"
#include <FreeRTOS.h>
#include <stdarg.h>
#include <cstdio>
#include <lwip/netif.h>
#include <task.h>
#include <lwip/init.h>
#include <lwip/tcpip.h>
#include <netif/tapif.h>
#include <lwip/dns.h>

extern "C" void app_main();

void main_start_task(void*) {
	app_main();
	vTaskDelete(nullptr);
}

extern "C" void vAssertCalled(const char* m, int l) {
	ESP_LOGE("assert", "file %s line %d", m, l);
	while (1) {}
}

extern "C" void lwip_assert_called(const char *msg) {
	ESP_LOGE("lwip", "%s", msg);
	while (1) {}
}

extern "C" void lwip_dbg_message(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

struct netif netif;

void finished(void *) {
		// DO LWIP NETIF INIT
		ip4_addr_t ipaddr, netmask, gw;
		const char *ipaddr_str = getenv("SIMESP_IP");
		const char *netmask_str = getenv("SIMESP_NETMASK");
		const char *gw_str = getenv("SIMESP_GATEWAY");

		if (!ipaddr_str || !netmask_str || !gw_str) {
			perror("missing ip cfg in env");
		}

		if (!ip4addr_aton(ipaddr_str, &ipaddr) ||
		    !ip4addr_aton(netmask_str, &netmask) ||
		    !ip4addr_aton(gw_str, &gw)) {
			perror("invalid ip addr");
		}

		netif_add(&netif, &ipaddr, &netmask, &gw, NULL, tapif_init, tcpip_input);
		netif_set_up(&netif);
		netif_set_default(&netif);

		//netif_set_addr(&netif, &ipaddr, &netmask, &gw);

		ip4_addr_t dns;
		IP4_ADDR(&dns, 1, 1, 1, 1);
		dns_setserver(0, &dns);
}

int main() {
	tcpip_init(finished, NULL);

	xTaskCreate(main_start_task, "uiT", 1024, nullptr, 10, nullptr);
	vTaskStartScheduler();
}
