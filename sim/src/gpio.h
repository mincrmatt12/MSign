#ifndef GPIO_H
#define GPIO_h

#include <stdint.h>

namespace gpio {
	template<uint32_t Port, uint32_t Mask>
	struct Pin {
		inline Pin& operator=(const bool& on) {
			return *this;
		}
	};
}

#endif
