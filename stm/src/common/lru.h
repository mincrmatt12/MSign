#ifndef LRU_H
#define LRU_H

#include <stdint.h>
#include <stddef.h>
#include <type_traits>

namespace lru {
	template<size_t Buckets, size_t ChainLength>
	struct Cache {
		static_assert(!(Buckets & (Buckets - 1)), "Buckets must be power of two");

		bool contains(uint16_t key) const {
			for (int i = 0; i < ChainLength; ++i) {
				if (bucket_for(key)[i] >> 16 == key) return true;
			}
			return false;
		}

		// Lookup while updating LRU
		uint16_t lookup(uint16_t key) {
			int i = 0;
			for (; i < ChainLength; ++i) {
				if (bucket_for(key)[i] >> 16 == key) break;
			}
			if (i == ChainLength) return 0;

			if (i) std::swap(bucket_for(key)[0], bucket_for(key)[i]);
			return bucket_for(key)[0] & 0xffff;
		}

		// Lookup without updating LRU
		uint16_t lookup(uint16_t key) const {
			for (int i = 0; i < ChainLength; ++i) {
				if (bucket_for(key)[i] >> 16 == key) {
					return bucket_for(key)[i] & 0xffff;
				}
			}
			return 0;
		}

		// Evict all values
		void evict() {
			memset(values, 0, sizeof values);
		}
		// Evict value for key
		void evict(uint16_t key) {
			for (int i = 0; i < ChainLength; ++i) {
				if (bucket_for(key)[i] >> 16 == key) bucket_for(key)[i] = 0;
			}
		}
		// Evict value for key if it matches a value
		void evict_if(uint16_t key, uint16_t invalidated) {
			for (int i = 0; i < ChainLength; ++i) {
				if (bucket_for(key)[i] == ((static_cast<uint32_t>(key) << 16) | invalidated)) bucket_for(key)[i] = 0;
			}
		}

		// Set the value for a key
		void insert(uint16_t key, uint16_t value) {
			int i = 0;
			for (; i < ChainLength; ++i) {
				if (bucket_for(key)[i] == 0) break;
			}
			if (i == ChainLength) {
				--i;
			}
			if (i) std::swap(bucket_for(key)[0], bucket_for(key)[i]);
			bucket_for(key)[0] = (static_cast<uint32_t>(key) << 16) | value;
		}

		// Either retrieve the value for a key or calculate it and insert it with a provided functor.
		template<typename Func>
		uint16_t lookup_or_calculate(uint16_t key, Func&& provider) {
			if (contains(key)) return lookup(key);
			else {
				uint16_t value = provider();
				insert(key, value);
				return value;
			}
		}

	private:
		uint32_t* bucket_for(uint16_t key) {
			const uint16_t constant = 40503;
			constexpr int shift = 16 - __builtin_ctz(Buckets);
			return values[static_cast<uint16_t>(key * constant) >> shift];
		}

		const uint32_t* bucket_for(uint16_t key) const {
			const uint16_t constant = 40503;
			constexpr int shift = 16 - __builtin_ctz(Buckets);
			return values[static_cast<uint16_t>(key * constant) >> shift];
		}

		uint32_t values[Buckets][ChainLength]{};
	};
}

#endif
