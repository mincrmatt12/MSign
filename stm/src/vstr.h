#ifndef VSTR_H
#define VSTR_H

#include "srv.h"
#include "common/slots.h"
#include <string.h>

extern srv::Servicer servicer;
extern uint64_t rtc_time;

namespace srv::vstr {
	// VStr's can contain any kind of data, the template is what to cast the pointer buffer to.
	
	const inline uint8_t buf_blocks = 32;
	const inline size_t buf_block_size = 64;
	extern uint8_t buf_data[buf_blocks][buf_block_size];
	extern bool buf_usage[buf_blocks];

	template<size_t Len>
	struct VSBlock {
		const static size_t length = Len;
		const static int num_segments = (Len / buf_block_size) + (Len % buf_block_size != 0);

		inline static uint8_t * open() {
			for (int i = 0; i < buf_blocks; ++i) {
				if (buf_usage[i]) continue;
				int j = i;
				for (;i < j + num_segments; ++i) {
					if (buf_usage[i]) goto repeatloop;
				}
				for (i = j; i < j + num_segments; ++i) {
					buf_usage[i] = true;
				}
				return static_cast<uint8_t *>(&buf_data[j][0]);
			repeatloop:
				;
			}

			return nullptr;
		}

		inline static void close(uint8_t * exist) {
			int j = ((exist - buf_data[0]) / buf_block_size);
			for (int i = j; i < j + num_segments; ++j) {
				buf_usage[i] = false;
			}
		}
	};

	template<uint8_t *Ptr, size_t Len>
	struct StaticStore {
		const static size_t length = Len;
		
		inline static uint8_t * open() {
			return Ptr;
		}

		inline static void close(uint8_t *) {}
	};

	template<typename T, typename Allocator=VSBlock<128>>
	struct BasicVSWrapper {
		const T* data = nullptr;  // can be null
		const static size_t length = Allocator::length;

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
					raw = Allocator::open();
					memset(raw, 0, length);
					if (!servicer.slot_connected(handle)) return false;
					servicer.ack_slot(handle);
					state = 2;
					return false;
				case 2:
					if (servicer.slot_dirty(handle, true)) {
						const auto& vs = servicer.slot<slots::VStr>(handle); // make sure we copy this in case there's some voodoo
						if (vs.index > vs.size) return false;

						if (vs.size >= length) {
							state = 0; // too large error
							data = nullptr;
							return true;
						}

						if (vs.size - vs.index <= 14) {
							size_t size = (vs.size - vs.index);
							if (size > 14) size = 14;
							if (vs.index + size > length) return false;
							state = 0;
							memcpy((raw + vs.index), vs.data, size);
							data = (T *)&raw;
							return true;
						}

						memcpy((raw + vs.index), vs.data, 14);
						servicer.ack_slot(handle);
						return false;
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
			Allocator::close(raw);
			state = 0;
		}

	private:
		uint8_t state = 0, handle;
		uint8_t * raw;
	}; 

	using VSWrapper = BasicVSWrapper<char>;
	template<typename T, size_t Len=128>
	using SizeVSWrapper = BasicVSWrapper<T, VSBlock<Len>>;
}

#endif
