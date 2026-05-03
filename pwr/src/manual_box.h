#pragma once

#include <utility>
#include <new>

namespace util {
	template<typename T>
	union manual_box {
		manual_box() {}
		~manual_box() {
			inner.~T();
		}
		
		template<typename ...Args>
		void init(Args&& ...args) {
			new (static_cast<void *>(&inner)) T{std::forward<Args>(args)...};
		}

		auto *operator&(this auto&& self) {
			return &self.inner;
		}

		auto operator->(this auto&& self) {
			return &self.inner;
		}

		decltype(auto) operator*(this auto&& self) {
			return self.inner;
		}
	private:
		T inner;
	};
}

