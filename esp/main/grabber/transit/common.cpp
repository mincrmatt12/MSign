#include "common.cfg.h"
#include "../../serial.h"
#include <type_traits>
#include <esp_log.h>

void transit::BaseEntryStaticInfo::load(size_t i) {
	// If name is not set, clear out static params
	if (!name) {
		serial::interface.delete_slot(slots::TTC_NAME_1 + i);
	}
	else {
		// Otherwise, load params (just name for now)
		serial::interface.update_slot_nosync(slots::TTC_NAME_1 + i, name.get());
	}
}

void transit::BaseEntryStaticInfos::load() {
	for (int i = 0; i < std::extent_v<decltype(entries), 0>; ++i) {
		entries[i].load(i);
	}
	// sync with serial so we can use nosync variants
	serial::interface.sync();
}

void transit::load_static_info(const char *impl) {
	BaseEntryStaticInfos infos;
	if (!load_name_list(impl, infos)) {
		ESP_LOGW("transitcommon", "Failed to load static infos");
	}

	infos.load();
}
