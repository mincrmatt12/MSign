#include "sccfg.h"
#include "../serial.h"
#include "../config.h"
#include <string.h>
#include "../wifitime.h"
#include "esp_log.h"
#include "sccfg.cfg.h"

const static char * const TAG = "sccfg";

namespace sccfg {

	uint16_t force_enabled_mask = 0;
	uint16_t force_disabled_mask = 0;
	uint16_t parsed_enabled_mask = (1 << number_of_screens) - 1;

	uint32_t current_timeat() {
		return wifi::get_localtime() % 86400000;
	}

	void send_mask() {
		uint16_t enabled_mask = (parsed_enabled_mask | force_enabled_mask) & ~force_disabled_mask;
		slots::ScCfgInfo obj;
		obj.enabled_mask = enabled_mask;

		ESP_LOGD(TAG, "enabled data: %d", enabled_mask);

		serial::interface.update_slot(slots::SCCFG_INFO, obj);
	}
	
	void init() {
		parsed_enabled_mask = 0;

		slots::ScCfgTime times[8];
		int number_configured = 0;

		for (; number_configured < number_of_screens && screen_entries[number_configured]; ++number_configured) {
			// Set enabled
			parsed_enabled_mask |= 1 << screen_entries[number_configured]->screen;
			// Add into order
			times[number_configured].screen_id = screen_entries[number_configured]->screen;
			times[number_configured].millis_enabled = screen_entries[number_configured]->duration;
		}

		serial::interface.update_slot_raw(slots::SCCFG_TIMING, &times[0], sizeof(slots::ScCfgTime)*number_configured);
	}

	void set_force_disable_screen(slots::ScCfgInfo::EnabledMask e, bool on) {
		auto old = force_disabled_mask;

		if (on) force_disabled_mask |= e;
		else force_disabled_mask &= ~(uint16_t)(e);

		if (force_disabled_mask == old) return;

		send_mask();
	}

	void set_force_enable_screen(slots::ScCfgInfo::EnabledMask e, bool on) {
		auto old = force_enabled_mask;

		if (on) force_enabled_mask |= e;
		else force_enabled_mask &= ~(uint16_t)(e);

		if (force_enabled_mask == old) return;

		send_mask();
	}

	bool loop() {
		auto curtime = current_timeat();

		for (int i = 0; i < number_of_screens && screen_entries[i]; ++i) {
			auto& cfg = *screen_entries[i];
			if (cfg.times.always_on()) continue;
			else if (cfg.times.on_for(curtime)) parsed_enabled_mask |= (1 << cfg.screen);
			else                                parsed_enabled_mask &= ~(1 << cfg.screen);
		}

		send_mask();
		return true;
	}

}
