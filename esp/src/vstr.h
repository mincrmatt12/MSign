#ifndef MS_VSTR_H
#define MS_VSTR_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "serial.h"
#include "common/slots.h"

namespace serial {
	struct VStrSender {
		ptrdiff_t echo_index = 0;
		const uint8_t * data = nullptr;
		size_t data_size = 0;
		
		void set(uint8_t * dat, size_t data_size) {
			data = dat;
			this->data_size = data_size;
			echo_index = 0;
		}

		void operator()(uint8_t * buffer, uint8_t & length) {
			slots::VStr vsw;
			
			if (data_size < echo_index) {
				echo_index = 0;
			}

			vsw.index = echo_index;
			vsw.size =  data_size;
			if (data_size - echo_index > 14) {
				memcpy(vsw.data, (data + vsw.index), 14);
				echo_index += 14;
			}
			else {
				memcpy(vsw.data, (data + vsw.index), (data_size - echo_index));
				echo_index = 0;
			}

			memcpy(buffer, &vsw, sizeof(vsw));
			length = sizeof(vsw);
		}
	};
}

#endif
