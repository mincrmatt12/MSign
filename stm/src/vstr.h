#ifndef VSTR_H
#define VSTR_H

#include "srv.h"
#include "common/slots.h"

extern srv::Servicer servicer;

namespace srv::vstr {
	// VStr's can contain any kind of data, the template is what to cast the pointer buffer to.

	template<typename T, std::size_t Len=128>
	struct BasicVSWrapper {
		const T* data = nullptr;  // can be null

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
					state = 2;
					servicer.ack_slot(handle);
					return false;
				case 2:
					if (servicer.slot_dirty(handle, true)) {
						const auto& vs = servicer.slot<slots::VStr>(handle);
						if (vs.size > Len) {
							state = 0; // too large error
							data = &raw;
							return true;
						}

						uint8_t size = 14;
						if (vs.size - vs.index <= 14) {
							size = (vs.size - vs.index);
							state = 0;
							memcpy((raw + vs.index), vs.data, size);
							data = &raw;
							return true;
						}

						memcpy((raw + vs.index), vs.data, size);
						servicer.ack_slot(handle);
						return false;
					}
					[[fallthrough]];
				default:
					return false;
			}
		}
		void renew() {
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
	}; 

	using VSWrapper = BasicVSWrapper<char>;
}

#endif
