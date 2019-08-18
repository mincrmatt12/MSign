#ifndef VSTR_H
#define VSTR_H

#include "srv.h"
#include "common/slots.h"
#include <string.h>

extern srv::Servicer servicer;
extern uint64_t rtc_time;

namespace srv::vstr {
	// VStr's can contain any kind of data, the template is what to cast the pointer buffer to.

	template<typename T, std::size_t Len=128>
	struct BasicVSWrapper {
		const T* data = nullptr;  // can be null

		bool is_updating() {return state > 0;}
		bool open(uint16_t sid) {
			if (servicer.open_slot(sid, false, handle)) {
				state = 1;
				return true;
			}
			return false;
		}
		bool update() {
			switch (state) {
				case 0:
					return true;
				case 1:
					memset(raw, 0, Len);
					if (!servicer.slot_connected(handle)) return false;
					servicer.ack_slot(handle);
					state = 2;
					first_flag = true;
					last_time = rtc_time;
					return false;
				case 2:
					if (servicer.slot_dirty(handle, true)) {
						last_time = rtc_time;
						const auto& vs = servicer.slot<slots::VStr>(handle); // make sure we copy this in case there's some voodoo
						if (vs.index > vs.size) return false;

						if (vs.size >= Len) {
							state = 0; // too large error
							data = nullptr;
							return true;
						}

						if (first_flag && vs.index != 0) {
							servicer.ack_slot(handle);
							return false;
						}
						first_flag = false;

						if (vs.size - vs.index <= 14) {
							size_t size = (vs.size - vs.index);
							state = 0;
							memcpy((raw + vs.index), vs.data, size);
							data = (T *)&raw;
							return true;
						}

						memcpy((raw + vs.index), vs.data, 14);
						servicer.ack_slot(handle);
						return false;
					}
					if (last_time - rtc_time > 50) {
						last_time = rtc_time;
						servicer.ack_slot(handle);
						first_flag = true;
					}
					[[fallthrough]];
				default:
					return false;
			}
		}
		void renew() {
			if (is_updating()) return;
			state = 1;
			data = nullptr;
		}
		void close() {
			servicer.close_slot(handle);
			data = nullptr;
			state = 0;
		}

	private:
		uint8_t state = 0, handle;
		uint8_t raw[Len];
		uint64_t last_time = 0;
		bool first_flag = true;
	}; 

	using VSWrapper = BasicVSWrapper<char>;
}

#endif
