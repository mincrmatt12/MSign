#ifndef MSN_PROTOCOL_H
#define MSN_PROTOCOL_H

// Re-pluggable protocol layer:
//
// 	Contains simple functions to wait for a packet header and wait for packet body.

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include "common/slots.h"

namespace protocol {
	struct ProtocolImpl {
		void init_hw(); // Setup hardware settings and begin RX processing task.
						// This entire class will call on_pkt whenever it gets a packet from a
						// blockable thread.

		void send_pkt(const void *packet); // Send a packet out blocking.
		inline void send_pkt(const slots::PacketWrapper<>& pw) {
			send_pkt(&pw);
		}

		bool wait_for_packet(TickType_t timeout=portMAX_DELAY);
	protected:
		union {
			uint8_t rx_buf[258];
			slots::PacketWrapper<255> rx_pkt;
		};

		unsigned get_processing_delay(); // return the number of milliseconds (rounded down) from when we received the header of the last packet.

	private:
		unsigned last_received_at;
	};
}

#endif
