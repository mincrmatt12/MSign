#include "gtfs.h"
#include "gtfs.cfg.h"
#include "ttc.cfg.h"
#include "common.cfg.h"

#include "../../serial.h"
#include "../../dwhttp.h"
#include "../../wifitime.h"

#include <gtfs_tripupdate.h>
#include "esp_log.h"

const static char * TAG = "gtfs";

namespace {
	struct TripParserState {
		slots::TTCInfo info;
		uint64_t times[3][6];
	};
}

extern "C" void gtfs_tripupdate_got_entry_hook(gtfs_tripupdate_state_t *state, uint8_t) {
	// Check if this matches any entry
	
	for (int i = 0; i < 3; ++i) {
		const auto& e = transit::gtfs::entries[i];
		if (e.route_id == nullptr || e.stop_id == nullptr) break;

		if (strcmp(state->c.route_id, e.route_id) == 0 && strcmp(state->c.stop_id, e.stop_id) == 0) {
			ESP_LOGD(TAG, "matched on %s-%s, %d", state->c.route_id, state->c.stop_id, (int)(state->c.at_time));
			auto& hstate = *reinterpret_cast<TripParserState *>(state->userptr);
			// Update entry
			auto minmax = std::minmax_element(hstate.times[i], hstate.times[i] + 6);
			uint64_t replace;
			if (*minmax.first == 0) {
				replace = 0;
			}
			else replace = *minmax.second;

			for (int j = 0; j < 6; ++j) {
				if (hstate.times[i][j] == replace) {
					hstate.times[i][j] = wifi::millis_to_local(state->c.at_time * 1000);
					break;
				}
			}

			// Resort array
			std::sort(hstate.times[i], hstate.times[i] + 6);

			// Update flags
			hstate.info.flags |= (hstate.info.EXIST_0 << i);
		}
	}
}

namespace transit::gtfs {
	void update_ttc_entry_name(size_t n, const char * v) {
		ttc::update_ttc_entry_name(n, v);
	}
	
	void init() {
	}

	bool loop() {
		if (transit::impl != GTFS) return true;

		// Start a request
		auto req = dwhttp::download_with_callback(feed_host, feed_url);

		if (!req.ok()) {
			ESP_LOGW(TAG, "failed to connect to gtfs host");
			return false;
		}

		if (req.is_unknown_length()) {
			ESP_LOGW(TAG, "unable to handle connection: close");
			return true; // avoid spam
		}

		auto remain = req.content_length();
		uint8_t buf[768];

		gtfs_tripupdate_state_t parser;
		TripParserState tps;

		memset(&tps, 0, sizeof tps);

		parser.userptr = &tps;
		gtfs_tripupdate_start(&parser);

		while (remain) {
			auto recvd = req(buf, std::min<size_t>(sizeof buf, remain));
			if (recvd == -1 || recvd == 0) {
				ESP_LOGE(TAG, "eof during dw");
				return false;
			}
			remain -= recvd;

			switch (gtfs_tripupdate_feed(buf, buf + recvd, &parser)) {
				case GTFS_TRIPUPDATE_OK:
					break;
				case GTFS_TRIPUPDATE_FAIL:
					ESP_LOGE(TAG, "failed to parse gtfs");
					return false;
				case GTFS_TRIPUPDATE_DONE:
					remain = 0;
					continue;
			}
		}

		ESP_LOGD(TAG, "parsed gtfs ok");

		for (int i = 0; i < 3; ++i) {
			if (!(tps.info.flags & (tps.info.EXIST_0 << i))) continue;

			serial::interface.update_slot_nosync(slots::TTC_TIME_1 + i, tps.times[i]);
		}

		serial::interface.update_slot(slots::TTC_INFO, tps.info);
		return true;
	}

}
