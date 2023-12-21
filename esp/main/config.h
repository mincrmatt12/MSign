#ifndef CONFIG_H
#define CONFIG_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <type_traits>
#ifndef __castxml__
#include "json.h"
#include <esp8266/eagle_soc.h>
#endif

namespace config {
	// Slightly less posessive unique_ptr, "lazy" as in "lazily allocated"
	template<typename T>
	struct lazy_t {
		static_assert(!std::is_array_v<T>, "lazy arrays do not work");

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

		lazy_t(){};
		lazy_t(const lazy_t& other) = delete;
		lazy_t(lazy_t&& other) {
			std::swap(ptr, other.ptr);
		}
		~lazy_t() {
			if (ptr) {
				dispose();
				ptr = nullptr;
			}
		}

		lazy_t& operator=(const lazy_t& other) = delete;
		lazy_t& operator=(lazy_t&& other) {
			std::swap(ptr, other.ptr);
			return (*this);
		}

		template<typename I>
		lazy_t& operator=(I x) {
			static_assert(std::is_same_v<std::decay_t<I>, T*>);

			if (ptr) dispose();
			ptr = (T*)x;
			return *this;
		}

		using inner = T;
	private:
		T* ptr{nullptr};

		inline void dispose() {
			if constexpr (std::is_array_v<T>) {
				delete[] ptr;
			}
			else {
				delete ptr;
			}
		}
	};

	struct string_t {
		// must be passed to constructor to get it to autofree
		struct heap_tag {};

		const char * get() const {return str;}
		operator const char *() const {return str;}
		operator bool() const {return str != nullptr;}

		bool operator==(const char *other) const {
			if (str && other && !strcmp(str, other)) return true;
			else return str == other;
		}

		~string_t() {dispose();}
		string_t() {}
		// takes ownership
		string_t(heap_tag tag, char * c) : str(c) {
			set_is_flash(false);
		}
		string_t(const char * c) : str(c) {
			set_is_flash(true);
		}
		string_t(const string_t& other) {
			(*this) = other;
		}
		string_t(string_t&& other) {
			(*this) = other;
		}

		string_t& operator=(const string_t& other) {
			dispose();
			if (other.not_heap()) {
				str = other.str;
				set_is_flash(true);
			}
			else {
				str = strdup(other.str);
				set_is_flash(false);
			}
			return *this;
		}

		string_t& operator=(string_t&& other) {
			std::swap(str, other.str);
#ifdef SIM
			std::swap(is_flash, other.is_flash);
#endif
			return *this;
		}

		bool not_heap() const {
			if (!str) return true;
#if defined(SIM)
			return is_flash;
#elif defined(__castxml__)
			return false;
#else
			return !(IS_DRAM(str) || IS_IRAM(str));
#endif
		}

	private:
		void set_is_flash(bool flag) {
#ifdef SIM
			is_flash = flag;
#endif
		}

		void dispose() {
			if (!not_heap() && str)
				free(const_cast<char *>(str));
			str = nullptr;
			set_is_flash(true);
		}

		const char * str{nullptr};
#ifdef SIM
		bool is_flash = false;
#endif
	};

#ifndef __castxml__
	json::TextCallback sd_cfg_load_source();
	bool parse_config(json::TextCallback&& tcb);
	bool parse_config_from_sd();
#endif
}

#endif
