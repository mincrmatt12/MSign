#include <stdint.h>
#include "vstr.h"

namespace srv::vstr {
	uint8_t buf_data[buf_blocks][buf_block_size];
	bool buf_usage[buf_blocks] = {0};
};
