#ifndef MS_VSTR_H
#define MS_VSTR_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "serial.h"
#include "common/slots.h"
#include <HardwareSerial.h>
#include "util.h"

namespace serial {
	struct VStrSender {
		ptrdiff_t echo_index = 0;
		const uint8_t * data = nullptr;
		size_t data_size = 0;
		
		void set(uint8_t * dat, size_t data_size) {
			data = dat;
			this->data_size = data_size;
			this->echo_index = 0;
			Log.println(F("vstr reset"));
		}

		void operator()(uint8_t * buffer, uint8_t & length) {
			slots::VStr vsw;
			
			if (data_size < static_cast<size_t>(echo_index)) {
				this->echo_index = 0;
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

			Log.printf("vstr: %d %d\n", vsw.index, vsw.size);

			memcpy(buffer, &vsw, 16);
			length = 16;
		}
	};
}

#endif
