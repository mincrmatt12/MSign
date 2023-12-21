#include "parcels.h"
#include "../serial.h"
#include "../common/slots.h"
#include "sccfg.h"
#include "../wifitime.h"
#include <esp_log.h>
#include "parcels.cfg.h"
#include <time.h>

static const char * const TAG = "parcels";

#define BASE_URL "/track/v2.2"

namespace parcels {
	struct ParcelDynamicInfo {
		int16_t name_offset{};

		enum State {
			UNKNOWN,                 // just read from config, unknown server-side state
			UNREGISTERED,            // not registered with 17track
			CHANGE_CARRIER,          // registered but with wrong carrier, needs changecarrier
			CHANGE_CARRIER_AND_INFO, // registered but with wrong carrier, needs changecarrier & changeregistrationinfo
			INCORRECT,               // registered but with incorrect lang/param, needs changeregistrationinfo

			READY,                   // ready to get track results
			MISSING,                 // slot unused in config
			FAILED,                  // failed to register
			EXPIRED_WRONGLANG        // tracking has stopped and language is wrong; use untranslated messages (can't change without wasting retrack)
		} state = MISSING;

		bool requires_initialization() const {return state < READY;}
		bool requires_changeinfo() const {return state <= INCORRECT && state >= CHANGE_CARRIER_AND_INFO;}
	} parcel_dynamic_infos[6];

	bool needs_reinit = true;

	void set_parcel_name(size_t i, const char * value) {
		if (i == 0) {
			for (auto& pdi : parcel_dynamic_infos) pdi.name_offset = 0;
			serial::interface.delete_slot(slots::PARCEL_NAMES);
		}
		parcel_dynamic_infos[i].name_offset = serial::interface.current_slot_size(slots::PARCEL_NAMES);
		serial::interface.allocate_slot_size(slots::PARCEL_NAMES, parcel_dynamic_infos[i].name_offset + strlen(value) + 1);
		serial::interface.update_slot_partial(slots::PARCEL_NAMES, parcel_dynamic_infos[i].name_offset, value, strlen(value) + 1);
	}

	void set_parcels_count(size_t length) {
		for (unsigned i = 0; i < length; ++i) {
			if (i < length) parcel_dynamic_infos[i].state = ParcelDynamicInfo::UNKNOWN;
			else            parcel_dynamic_infos[i].state = ParcelDynamicInfo::MISSING;
		}
	}

	auto generate_strappender(slots::DataID did) {
		return [did, offset = (size_t)0] (const char * text) mutable {
			if (text == nullptr) {
				serial::interface.allocate_slot_size(did, offset);
				serial::interface.trigger_slot_update(did);
				return offset;
			}
			size_t newend = offset + strlen(text) + 1;
			if (newend > serial::interface.current_slot_size(did)) serial::interface.allocate_slot_size(did, newend);
			serial::interface.update_slot_partial(did, offset, text, strlen(text) + 1, true, false);
			size_t result = offset;
			offset = newend;
			return result;
		};
	}

	slots::ParcelInfo::StatusIcon get_icon_enum(const char * text) {
#define START(pat, match) else if (!strncmp(pat, text, strlen(pat))) return slots::ParcelInfo::match
#define EXACT(pat, match) else if (!strcmp(pat, text)) return slots::ParcelInfo::match

		if (false) {}
		START("InfoReceived", PRE_TRANSIT);
		EXACT("InTransit_CustomsRequiringInformation", GENERAL_ERROR); // todo icon
		START("InTransit", IN_TRANSIT);
		START("OutForDelivery", OUT_FOR_DELIVERY);
		START("DeliveryFailure", FAILED_TO_DELIVER);
		EXACT("NotFound_InvalidCode", GENERAL_ERROR);
		START("Delivered", DELIVERED);
		START("AvailableForPickup", READY_FOR_PICKUP);
		EXACT("Exception_Returned", RETURN_TO_SENDER);
		EXACT("Exception_Returning", RETURN_TO_SENDER);
		EXACT("Exception_Cancel", CANCELLED);
		EXACT("Exception_Delayed", IN_TRANSIT);
		START("Exception", GENERAL_ERROR);

#undef START
#undef EXACT
		return slots::ParcelInfo::UNK;
	}

	struct LocationBuf {
		LocationBuf() : has_country(false), has_city(false) {}

		bool has_fallback() const { return fallback[0]; }

		const char *result() const {
			if (has_country && has_city) return generated;
			if (has_fallback()) return fallback;
			if (has_country || has_city) return generated;
			return nullptr;
		}

		void push_fallback(const char * c) {
			strncpy(fallback, c, sizeof fallback);
		}

		void push_country(const char * c) {
			if (has_country) return;
			has_country = true;
			if (!has_city) {
				strncpy(generated, c, sizeof generated);
			}
			else {
				// append
				int end = strlen(generated);
				snprintf(generated + end, sizeof generated - end, ", %s", c);
			}
		}

		void push_city(const char * c) {
			if (has_city) return;
			has_city = true;
			if (!has_country) {
				strncpy(generated, c, sizeof generated);
			}
			else {
				char buf2[sizeof generated]; strncpy(buf2, generated, sizeof buf2);
				snprintf(generated, sizeof generated, "%s, %s", c, buf2);
			}
		}

		// localize stack by stack + offs, stack_ptr - offs, etc.
		void push_from_event(json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
			if (stack_ptr == 1) {
				if (!strcmp(stack[0]->name, "location") && v.type == v.STR) push_fallback(v.str_val);
			}
			else if (stack_ptr == 2 && !strcmp(stack[0]->name, "address") && v.type == v.STR) {
				if (!strcmp(stack[1]->name, "country")) push_country(v.str_val);
				else if (!strcmp(stack[1]->name, "city")) push_city(v.str_val);
			}
		}
	private:
		char fallback[64]{};
		char generated[64]{};

		bool has_country : 1;
		bool has_city : 1;
	};

	const char * resolved_description_str(bool use_localized, json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
		if (v.type != json::Value::STR) return nullptr;
		if (use_localized && stack_ptr == 2 && !strcmp(stack[0]->name, "description_translation") && !strcmp(stack[1]->name, "description"))
			return v.str_val;
		else if (!use_localized && stack_ptr == 1 && !strcmp(stack[0]->name, "description"))
			return v.str_val;
		else
			return nullptr;
	}

	static_assert(std::is_trivially_destructible_v<LocationBuf>);

	constexpr inline size_t max_single_entry_textcount = 600;

	void authorize_request(dwhttp::Connection& conn, bool as_json=true) {
		if (conn.state() == dwhttp::Connection::SEND_HEADERS) {
			conn.send_header("17token", parcels_api_key);
			if (as_json) conn.send_header("Content-Type", "application/json");
		}
	}

	void common_log_17track_rejection(json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
		if (stack_ptr == 5 && !strcmp(stack[2]->name, "rejected")) {
			if (v.type == json::Value::INT) ESP_LOGW(TAG, "rejection %s: %d", stack[4]->name, (int)v.int_val);
			else if (v.type == json::Value::STR) ESP_LOGW(TAG, "rejection %s: %s", stack[4]->name, v.str_val);
		}
	}

	bool do_package_statemachine() {
		if (!parcels_api_key) return false;

		// First, go and scan all unknown packages with gettracklist.
		if (std::any_of(std::begin(parcel_dynamic_infos), std::end(parcel_dynamic_infos), [](const auto& x){return x.state == ParcelDynamicInfo::UNKNOWN;})) {
			auto request = dwhttp::open_site("_api.17track.net", BASE_URL "/gettracklist", "POST");
			authorize_request(request);

			request.start_body();
			request.write("{\"number\": \"");
			bool comma = false;
			for (int i = 0; i < 6; ++i) {
				if (parcel_dynamic_infos[i].state == ParcelDynamicInfo::UNKNOWN) {
					if (comma) request.write(",");
					comma = true;
					ESP_LOGD(TAG, "checking server status of %s", tracker_configs[i].tracking_number.get());
					request.write(tracker_configs[i].tracking_number);
				}
			}
			request.write("\"}\n");
			request.end_body();

			struct TrackListEntry {
				int match = -1;

				int carrier_code = 0, final_carrier_code = 0;
				char recorded_param[16]{};
				
				bool lang_ok = true;
				bool stopped = false;

				void finalize() {
					if (match == -1) return; // ignore irrelevant entries

					// record autodetection results if found
					if (tracker_configs[match].carrier_id == 0) tracker_configs[match].carrier_id = carrier_code;
					if (tracker_configs[match].final_carrier_id == 0) tracker_configs[match].final_carrier_id = final_carrier_code;

					bool cfg_ok = lang_ok && (tracker_configs[match].additional_param == nullptr || !strcmp(tracker_configs[match].additional_param, recorded_param));
					bool carrier_ok = tracker_configs[match].carrier_id == carrier_code && tracker_configs[match].final_carrier_id == final_carrier_code;

					if (stopped) {
						parcel_dynamic_infos[match].state = lang_ok ? ParcelDynamicInfo::READY : ParcelDynamicInfo::EXPIRED_WRONGLANG;
					}
					else {
						if (cfg_ok && carrier_ok) parcel_dynamic_infos[match].state = ParcelDynamicInfo::READY;
						else if (!carrier_ok) parcel_dynamic_infos[match].state = ParcelDynamicInfo::CHANGE_CARRIER;
						else parcel_dynamic_infos[match].state = ParcelDynamicInfo::INCORRECT;
					}
				}
			} current{};

			json::JSONParser parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
				if (stack_ptr == 3 && !strcmp(stack[1]->name, "data") && !strcmp(stack[2]->name, "accepted") && stack[2]->is_array() && v.type == json::Value::OBJ) {
					current.finalize();
					current.~TrackListEntry();
					new (&current) TrackListEntry{};
					return;
				}

				if (stack_ptr == 4 && !strcmp(stack[1]->name, "data") && !strcmp(stack[2]->name, "accepted") && stack[2]->is_array()) {
					if (!strcmp(stack[3]->name, "number") && v.type == json::Value::STR) {
						for (int i = 0; i < 6; ++i) {
							if (parcel_dynamic_infos[i].state != ParcelDynamicInfo::UNKNOWN) continue;
							if (!strcmp(v.str_val, tracker_configs[i].tracking_number)) current.match = i;
						}
					}
					else if (!strcmp(stack[3]->name, "lang")) {
						current.lang_ok = v.type == json::Value::STR && !strcmp(v.str_val, "en");
					}
					else if (!strcmp(stack[3]->name, "param") && v.type == v.STR) {
						strncpy(current.recorded_param, v.str_val, sizeof current.recorded_param);
					}
					else if (!strcmp(stack[3]->name, "carrier") && v.type == v.INT) {
						current.carrier_code = v.int_val;
					}
					else if (!strcmp(stack[3]->name, "final_carrier") && v.type == v.INT) {
						current.final_carrier_code = v.int_val;
					}
					else if (!strcmp(stack[3]->name, "tracking_status") && v.type == v.STR && !strcmp(v.str_val, "Stopped")) {
						current.stopped = true;
					}
				}
			}, true);

			if (!request.ok()) {
				ESP_LOGE(TAG, "Failed to gettracklist; API key bad?");
				return false;
			}

			request.make_nonclose();
			if (!parser.parse(request)) {
				ESP_LOGE(TAG, "Bad json into gettracklist");
				return false;
			}

			// Anything still unknown at this point needs registering.
			for (auto& e : parcel_dynamic_infos) if (e.state == ParcelDynamicInfo::UNKNOWN) e.state = ParcelDynamicInfo::UNREGISTERED;
		}

		// Check if anything needs registering.
		if (std::any_of(std::begin(parcel_dynamic_infos), std::end(parcel_dynamic_infos), [](const auto& x){return x.state == ParcelDynamicInfo::UNREGISTERED;})) {
			auto request = dwhttp::open_site("_api.17track.net", BASE_URL "/register", "POST");
			authorize_request(request);
			request.make_nonclose();

			ESP_LOGI(TAG, "New parcels detected in config, registering:");

			request.start_body();
			request.write("[");

			bool comma = false;
			char buf[32];

			for (int i = 0; i < 6; ++i) {
				if (parcel_dynamic_infos[i].state != ParcelDynamicInfo::UNREGISTERED) continue;
				request.write(comma ? ",{\"number\": \"" : "{\"number\": \"");
				comma = true;
				request.write(tracker_configs[i].tracking_number);
				ESP_LOGI(TAG, " - id %d; no: %s", i, tracker_configs[i].tracking_number.get());
				request.write(R"(", "lang": "en")");
				if (tracker_configs[i].carrier_id != 0) {
					snprintf(buf, sizeof buf, R"(, "carrier": %d)", tracker_configs[i].carrier_id);
					request.write(buf);
				}
				if (tracker_configs[i].final_carrier_id != 0) {
					snprintf(buf, sizeof buf, R"(, "final_carrier": %d)", tracker_configs[i].final_carrier_id);
					request.write(buf);
				}
				if (tracker_configs[i].additional_param) {
					request.write(R"(, "param": ")");
					request.write(tracker_configs[i].additional_param);
					request.write("\"");
				}
				request.write("}");
			}

			request.write("]");
			request.end_body();

			json::JSONParser parser([](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
				if (stack_ptr == 4 && !strcmp(stack[1]->name, "data") && !strcmp(stack[3]->name, "number") && v.type == v.STR) {
					ParcelDynamicInfo::State next_state = !strcmp(stack[2]->name, "accepted") ? ParcelDynamicInfo::READY : ParcelDynamicInfo::FAILED;
					for (int i = 0; i < 6; ++i) {
						if (parcel_dynamic_infos[i].state == ParcelDynamicInfo::MISSING) continue;
						if (!strcmp(v.str_val, tracker_configs[i].tracking_number)) {
							parcel_dynamic_infos[i].state = next_state;
							if (next_state == ParcelDynamicInfo::READY) {
								ESP_LOGI(TAG, "Succesfully registered package id %d", i);
							}
							else {
								ESP_LOGE(TAG, "Failed to register package id %d", i);
							}
						}
					}
				}

				common_log_17track_rejection(stack, stack_ptr, v);
			});

			if (!request.ok() || !parser.parse(request)) {
				ESP_LOGE(TAG, "Invalid response from 17track (code %d)", request.result_code());
				for (auto& e : parcel_dynamic_infos) {
					if (e.state == ParcelDynamicInfo::UNREGISTERED) e.state = ParcelDynamicInfo::FAILED;
				}
			}
		}

		// Check if anything needs changeregistrationinfo 
		if (std::any_of(std::begin(parcel_dynamic_infos), std::end(parcel_dynamic_infos), [](const auto& x){
			return x.requires_changeinfo();
		})) {
			auto request = dwhttp::open_site("_api.17track.net", BASE_URL "/changeinfo", "POST");
			authorize_request(request);
			request.make_nonclose();

			ESP_LOGI(TAG, "Parcels with wrong registration info (param, lang) detected, fixing");

			request.start_body();
			request.write("[");

			bool comma = false;

			for (int i = 0; i < 6; ++i) {
				if (!parcel_dynamic_infos[i].requires_changeinfo()) continue;
				request.write(comma ? ",{\"number\": \"" : "{\"number\": \"");
				comma = true;
				request.write(tracker_configs[i].tracking_number);
				ESP_LOGI(TAG, " - id %d; no: %s", i, tracker_configs[i].tracking_number.get());
				request.write(R"(", "items": {"lang": "en")");
				if (tracker_configs[i].additional_param) {
					request.write(R"(, "param": ")");
					request.write(tracker_configs[i].additional_param);
					request.write("\"");
				}
				request.write("}}");
			}

			request.write("]");
			request.end_body();

			json::JSONParser parser([](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
				if (stack_ptr == 4 && !strcmp(stack[1]->name, "data") && !strcmp(stack[3]->name, "number") && v.type == v.STR) {
					bool accepted = !strcmp(stack[2]->name, "accepted");
					for (int i = 0; i < 6; ++i) {
						if (!parcel_dynamic_infos[i].requires_changeinfo()) continue;
						if (!strcmp(v.str_val, tracker_configs[i].tracking_number)) {
							if (accepted) {
								ESP_LOGI(TAG, "Succesfully updated package id %d", i);
							}
							else {
								ESP_LOGW(TAG, "Failed to updated package id %d", i);
							}
							if (parcel_dynamic_infos[i].state == ParcelDynamicInfo::INCORRECT) parcel_dynamic_infos[i].state = ParcelDynamicInfo::READY;
							else parcel_dynamic_infos[i].state = ParcelDynamicInfo::CHANGE_CARRIER;
						}
					}
				}

				common_log_17track_rejection(stack, stack_ptr, v);
			});

			if (!request.ok() || !parser.parse(request)) {
				ESP_LOGE(TAG, "Invalid response from 17track (code %d)", request.result_code());
			}
		}

		// Check if anything needs changecarrier 
		if (std::any_of(std::begin(parcel_dynamic_infos), std::end(parcel_dynamic_infos), [](const auto& x){
			return x.state == ParcelDynamicInfo::CHANGE_CARRIER;
		})) {
			auto request = dwhttp::open_site("_api.17track.net", BASE_URL "/changecarrier", "POST");
			authorize_request(request);
			request.make_nonclose();

			ESP_LOGI(TAG, "Parcels with wrong carrier detected, fixing");

			request.start_body();
			request.write("[");

			bool comma = false;
			char buf[32];

			for (int i = 0; i < 6; ++i) {
				if (parcel_dynamic_infos[i].state != ParcelDynamicInfo::CHANGE_CARRIER) continue;
				request.write(comma ? ",{\"number\": \"" : "{\"number\": \"");
				comma = true;
				request.write(tracker_configs[i].tracking_number);
				ESP_LOGI(TAG, " - id %d; no: %s", i, tracker_configs[i].tracking_number.get());
				request.write("\"");
				if (tracker_configs[i].carrier_id != 0) {
					snprintf(buf, sizeof buf, R"(, "carrier_new": %d)", tracker_configs[i].carrier_id);
					request.write(buf);
				}
				if (tracker_configs[i].final_carrier_id != 0) {
					snprintf(buf, sizeof buf, R"(, "final_carrier_new": %d)", tracker_configs[i].final_carrier_id);
					request.write(buf);
				}
				request.write("}");
			}

			request.write("]");
			request.end_body();

			json::JSONParser parser([](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
				if (stack_ptr == 4 && !strcmp(stack[1]->name, "data") && !strcmp(stack[3]->name, "number") && v.type == v.STR) {
					bool accepted = !strcmp(stack[2]->name, "accepted");
					for (int i = 0; i < 6; ++i) {
						if (parcel_dynamic_infos[i].state != ParcelDynamicInfo::CHANGE_CARRIER) continue;
						if (!strcmp(v.str_val, tracker_configs[i].tracking_number)) {
							if (accepted) {
								ESP_LOGI(TAG, "Succesfully updated package id %d", i);
							}
							else {
								ESP_LOGW(TAG, "Failed to update package id %d", i);
							}
							parcel_dynamic_infos[i].state = ParcelDynamicInfo::READY;
						}
					}
				}

				common_log_17track_rejection(stack, stack_ptr, v);
			});

			if (!request.ok() || !parser.parse(request)) {
				ESP_LOGE(TAG, "Invalid response from 17track (code %d)", request.result_code());
			}
		}

		return true;
	}

	void init() {
		set_parcels_count(parcel_count);
		needs_reinit = parcel_count > 0;
	}
	bool loop() {
		if (!parcels_api_key) return true;
		if (needs_reinit) {
			needs_reinit = false;
			if (!do_package_statemachine()) {
				dwhttp::close_connection(true);
				return true;
			}
		}

		// If there are no configured packages, return early.
		if (parcel_count == 0 || parcel_dynamic_infos[0].state == ParcelDynamicInfo::MISSING) {
			sccfg::set_force_disable_screen(slots::ScCfgInfo::PARCELS, true);
			serial::interface.delete_slot(slots::PARCEL_INFOS);
			serial::interface.delete_slot(slots::PARCEL_EXTRA_INFOS);
			serial::interface.delete_slot(slots::PARCEL_STATUS_SHORT);
			serial::interface.delete_slot(slots::PARCEL_STATUS_LONG);

			return true;
		}

		auto append_shortheap = generate_strappender(slots::PARCEL_STATUS_SHORT), append_longheap = generate_strappender(slots::PARCEL_STATUS_LONG), 
			 append_carriers = generate_strappender(slots::PARCEL_CARRIER_NAMES);

		auto request = dwhttp::open_site("_api.17track.net", BASE_URL "/gettrackinfo", "POST");
		authorize_request(request);
		request.start_body();
		request.write("[");
		
		{
			bool comma = false;
			for (int i = 0; i < 6; ++i) {
				auto ps = parcel_dynamic_infos[i].state;
				if (!(ps == ParcelDynamicInfo::READY || ps == ParcelDynamicInfo::EXPIRED_WRONGLANG)) continue;

				request.write(comma ? ",{\"number\": \"" : "{\"number\": \"");
				comma = true;
				request.write(tracker_configs[i].tracking_number);
				request.write("\"}");
			}
		}

		request.write("]");
		request.end_body();

		int text_overflow_counter = 0;
		int p_idx = -1;
		bool text_overflow_effected = false;

		slots::ParcelInfo master_pis[6]{};
		slots::ExtraParcelInfoEntry current_epi{};
		LocationBuf lbuf;
		uint16_t last_provider_entry = 0;

		// Clear out current data for parcel infos
		serial::interface.delete_slot(slots::PARCEL_INFOS);

		size_t max_allocated_epis = serial::interface.current_slot_size(slots::PARCEL_EXTRA_INFOS);
		size_t epi_idx = 0;
		if (max_allocated_epis == 0) {
			// conservatively use at least 4
			max_allocated_epis = 4;
			serial::interface.allocate_slot_size(slots::PARCEL_EXTRA_INFOS, 4 * sizeof(slots::ExtraParcelInfoEntry));
		}
		else {
			max_allocated_epis /= sizeof(slots::ExtraParcelInfoEntry);
		}

		int num_packages = 0;
		int epis_for_current = 0;

		json::JSONParser parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
			common_log_17track_rejection(stack, stack_ptr, v);
			if (stack_ptr < 3) return;
			if (strcmp(stack[1]->name, "data")) return;
			if (strcmp(stack[2]->name, "accepted") || !stack[2]->is_array()) return;

			int s_idx = stack[2]->index;
			if (s_idx > 6) return;

			auto& parcel_info = master_pis[s_idx];

			if (stack_ptr == 3 && v.type == v.OBJ) {
				if (p_idx < 0) ESP_LOGW(TAG, "Didn't determine package ID in time");
				// reset for next package
				text_overflow_counter = 0;
				num_packages = s_idx + 1;
				p_idx = -1;
				text_overflow_effected = false;
				if (epis_for_current == 0) parcel_info.status.flags |= slots::ParcelStatusLine::EXTRA_INFO_MISSING;
				epis_for_current = 0;
				lbuf.~LocationBuf();
				new (&lbuf) LocationBuf{};
			}
			
			// Associate parcel number with name
			if (stack_ptr == 4) {
				if (!strcmp(stack[3]->name, "number") && v.type == v.STR) {
					for (int i = 0; i < 6; ++i) {
						if (tracker_configs[i].tracking_number && !strcmp(tracker_configs[i].tracking_number, v.str_val)) p_idx = i;
					}
					ESP_LOGD(TAG, "Started parcel data (id %d) for %s", p_idx, v.str_val);
					// Fill in parcel_info with constant data.
					parcel_info.name_offset = parcel_dynamic_infos[p_idx].name_offset;
				}
			}
			else if (!strcmp(stack[3]->name, "track_info")) {
				if (p_idx < 0) return;
				const auto& dynamic_info = parcel_dynamic_infos[p_idx];
				bool use_localized = dynamic_info.state == ParcelDynamicInfo::READY;

				if (stack_ptr >= 6 && !strcmp(stack[4]->name, "time_metrics") && !strcmp(stack[5]->name, "estimated_delivery_date")) {
					if (stack_ptr == 7 && v.type == json::Value::STR) { // record timestamps
						if (!strcmp(stack[6]->name, "from"))
							parcel_info.estimated_delivery_from = wifi::from_iso8601(v.str_val, true, true);
						if (!strcmp(stack[6]->name, "to"))
							parcel_info.estimated_delivery_to = wifi::from_iso8601(v.str_val, true);
					}
					if (stack_ptr == 6 && v.type == json::Value::OBJ) { // finalize estimated date info with flags and move from-->to
						if (parcel_info.estimated_delivery_from && parcel_info.estimated_delivery_to) {
							parcel_info.status.flags |= slots::ParcelStatusLine::HAS_EST_DEILIVERY | slots::ParcelStatusLine::HAS_EST_DELIVERY_RANGE;
						}
						else if (parcel_info.estimated_delivery_from) {
							parcel_info.estimated_delivery_to = std::exchange(parcel_info.estimated_delivery_from, 0);
							parcel_info.status.flags |= slots::ParcelStatusLine::HAS_EST_DEILIVERY;
						}
						else if (parcel_info.estimated_delivery_to) {
							parcel_info.status.flags |= slots::ParcelStatusLine::HAS_EST_DEILIVERY;
						}
					}
				}
				else if (stack_ptr >= 5 && !strcmp(stack[4]->name, "latest_event")) {
					if (stack_ptr == 5 && v.type == v.OBJ) {
						if (const char *loc = lbuf.result()) {
							parcel_info.status.location_offset = append_shortheap(loc);
							parcel_info.status.flags |= slots::ParcelStatusLine::HAS_LOCATION;
						}
						lbuf.~LocationBuf();
						new (&lbuf) LocationBuf{};
					}
					else {
						// Update location info
						lbuf.push_from_event(stack + 5, stack_ptr - 5, v);
						// Update status
						if (const auto* desc_str = resolved_description_str(use_localized, stack + 5, stack_ptr - 5, v)) {
							parcel_info.status.status_offset = append_shortheap(desc_str);
							parcel_info.status.flags |= slots::ParcelStatusLine::HAS_STATUS;
						}
						// Other metadata
						else if (stack_ptr == 6) {
							if (!strcmp(stack[5]->name, "time_utc") && v.type == v.STR) {
								parcel_info.updated_time = wifi::from_iso8601(v.str_val);
								parcel_info.status.flags |= slots::ParcelStatusLine::HAS_UPDATED_TIME;
							}
							else if (!strcmp(stack[5]->name, "sub_status") && v.type == v.STR) {
								parcel_info.status_icon = get_icon_enum(v.str_val);
							}
						}
					}
				}
				else if (stack_ptr >= 6 && !strcmp(stack[4]->name, "tracking") && !strcmp(stack[5]->name, "providers")) {
					// If we've overflowed the text counter, ignore any events.
					if (text_overflow_effected) return;
					if (stack_ptr == 8 && !strcmp(stack[6]->name, "provider") && !strcmp(stack[7]->name, "name") && v.type == v.STR) {
						last_provider_entry = append_carriers(v.str_val);
					}
					else if (stack_ptr >= 7 && !strcmp(stack[6]->name, "events") && stack[6]->is_array()) {
						if (stack_ptr == 7 && v.type == v.OBJ) {
							current_epi.for_parcel = s_idx;
							if (stack[6]->index == 0) {
								current_epi.new_subcarrier_offset = std::exchange(last_provider_entry, 0);
								current_epi.status.flags |= slots::ParcelStatusLine::HAS_NEW_CARRIER;
							}
							// fetch location info
							if (const char *loc = lbuf.result()) {
								current_epi.status.location_offset = append_longheap(loc);
								current_epi.status.flags |= slots::ParcelStatusLine::HAS_LOCATION;
							}
							if (epi_idx == max_allocated_epis) {
								max_allocated_epis += 4;
								serial::interface.allocate_slot_size(slots::PARCEL_EXTRA_INFOS, max_allocated_epis * sizeof(slots::ExtraParcelInfoEntry));
							}
							serial::interface.update_slot_at(slots::PARCEL_EXTRA_INFOS, current_epi, epi_idx++, true, false);
							current_epi.~ExtraParcelInfoEntry();
							new (&current_epi) slots::ExtraParcelInfoEntry{};
							lbuf.~LocationBuf();
							new (&lbuf) LocationBuf{};
							++epis_for_current;

							if (text_overflow_counter > max_single_entry_textcount) {
								text_overflow_effected = true;
								parcel_info.status.flags |= slots::ParcelStatusLine::EXTRA_INFO_TRUNCATED;
							}
						}
						else if (stack_ptr >= 8) {
							lbuf.push_from_event(stack + 7, stack_ptr - 7, v);
							if (const auto* desc_str = resolved_description_str(use_localized, stack + 7, stack_ptr - 7, v)) {
								text_overflow_counter += strlen(desc_str);
								current_epi.status.status_offset = append_longheap(desc_str);
								current_epi.status.flags |= slots::ParcelStatusLine::HAS_STATUS;
							}
							else if (stack_ptr == 8) {
								if (!strcmp(stack[7]->name, "time_utc") && v.type == v.STR) {
									current_epi.updated_time = wifi::from_iso8601(v.str_val);
									current_epi.status.flags |= slots::ParcelStatusLine::HAS_UPDATED_TIME;
								}
							}
						}
					}
				}
			}
		}, true);

		if (!request.ok() || !parser.parse(request)) {
			ESP_LOGE(TAG, "gettrackinfo failed (code %d)", request.result_code());
			return false;
		}

		if (num_packages == 0) {
			// no parcels detected.
			sccfg::set_force_disable_screen(slots::ScCfgInfo::PARCELS, true);
			serial::interface.delete_slot(slots::PARCEL_INFOS);
			serial::interface.delete_slot(slots::PARCEL_EXTRA_INFOS);
			serial::interface.delete_slot(slots::PARCEL_STATUS_SHORT);
			serial::interface.delete_slot(slots::PARCEL_STATUS_LONG);

			return true;
		}
		else {
			sccfg::set_force_disable_screen(slots::ScCfgInfo::PARCELS, false);
		}

		// truncate everything
		append_carriers(nullptr);
		append_shortheap(nullptr);
		append_longheap(nullptr);
		serial::interface.allocate_slot_size(slots::PARCEL_EXTRA_INFOS, epi_idx * sizeof(slots::ExtraParcelInfoEntry));
		serial::interface.trigger_slot_update(slots::PARCEL_EXTRA_INFOS);
		serial::interface.update_slot_raw(slots::PARCEL_INFOS, master_pis, sizeof(slots::ParcelInfo) * num_packages);

		dwhttp::close_connection(true);
		return true;
	}
}
