#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "json.h"

namespace config {
	template<typename T>
	struct lazy_t {
		const T* get() const {return ptr;}
		const T* get(const T& def) const {return (*this) ? get() : def;}

		T* get() {return ptr;}
		T* get(const T& def) {return (*this) ? get() : def;}

		operator bool() const {return ptr != nullptr;}
		operator const T*() const {return ptr;}

		operator T*() {return ptr;}

		T* operator->() {return ptr;}
		T& operator*() {return *ptr;}

		const T* operator->() const {return ptr;}
		const T& operator*() const {return *ptr;}

		~lazy_t() {
			if (ptr) {
				delete ptr;
				ptr = nullptr;
			}
		}

		template<typename I>
		lazy_t& operator=(const I& x) {
			static_assert(std::is_same_v<std::decay_t<I>, T*> || (std::is_array_v<T> && std::is_same_v<std::decay_t<I>, std::decay_t<T>>));

			if (ptr) delete ptr;
			ptr = x;
		}

		using inner = T;
	private:
		T* ptr{nullptr};
	};

	void parse_config(json::TextCallback&& tcb);
}

#endif
