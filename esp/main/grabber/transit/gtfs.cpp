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
		uint64_t times_a[5][6];
		uint64_t times_b[5][6];
	};
}

extern "C" void gtfs_tripupdate_got_entry_hook(gtfs_tripupdate_state_t *state, uint8_t) {
	// Check if this matches any entry
	
	int which_slot = 0;
	for (const auto& e_ : transit::gtfs::entries) {
		if (!e_ || !*e_) break;
		const auto& e = *e_;
		if (e.route_id == nullptr || e.stop_id == nullptr) break;

		auto& hstate = *reinterpret_cast<TripParserState *>(state->userptr);
		
		bool matched_a = strcmp(state->c.route_id, e.route_id) == 0 && strcmp(state->c.stop_id, e.stop_id) == 0;
		bool matched_b = e.has_alt() && strcmp(state->c.route_id, e.route_id_alt) == 0 && strcmp(state->c.stop_id, e.stop_id_alt) == 0;

		if (matched_a || matched_b) {
			ESP_LOGD(TAG, "matched on %s-%s, %d", state->c.route_id, state->c.stop_id, (int)(state->c.at_time));

			auto& times = matched_a ? hstate.times_a[which_slot] : hstate.times_b[which_slot];

			// Update entry
			auto minmax = std::minmax_element(times, times + 6);
			uint64_t replace;
			if (*minmax.first == 0) {
				replace = 0;
			}
			else replace = *minmax.second;

			for (int j = 0; j < 6; ++j) {
				if (times[j] == replace) {
					times[j] = wifi::millis_to_local(state->c.at_time * 1000);
					break;
				}
			}

			// Resort array
			std::sort(times, times + 6, [](auto a, auto b){
				if (a == 0) return false;
				if (b == 0) return true;
				return a < b;
			});

			// Update flags
			hstate.info.flags |= (hstate.info.EXIST_0 << which_slot);
		}

		++which_slot;
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

		for (int i = 0; i < 5; ++i) {
			if (!(tps.info.flags & (tps.info.EXIST_0 << i))) continue;

			if (tps.times_a[i][0])
				serial::interface.update_slot_raw(slots::TTC_TIME_1a + i, tps.times_a[i], sizeof(uint64_t) * std::count_if(std::begin(tps.times_a[i]), std::end(tps.times_a[i]), [](auto x){return x != 0;}), false);
			else serial::interface.delete_slot(slots::TTC_TIME_1a + i);
			if (tps.times_b[i][0])
				serial::interface.update_slot_raw(slots::TTC_TIME_1b + i, tps.times_b[i], sizeof(uint64_t) * std::count_if(std::begin(tps.times_b[i]), std::end(tps.times_b[i]), [](auto x){return x != 0;}), false);
			else serial::interface.delete_slot(slots::TTC_TIME_1b + i);

			// set slot altcodes
			tps.info.altdircodes_a[i] = (char)entries[i]->dir_a;
			tps.info.altdircodes_b[i] = (char)entries[i]->dir_b;
		}

		serial::interface.update_slot(slots::TTC_INFO, tps.info);
		return true;
	}

}
