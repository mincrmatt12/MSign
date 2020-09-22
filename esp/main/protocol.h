#ifndef MSN_PROTOCOL_H
#define MSN_PROTOCOL_H

// Re-pluggable protocol layer:
//
// 	Contains simple functions to wait for a packet header and wait for packet body.

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>

namespace protocol {
	struct ProtocolImpl {
		void init_hw(); // Setup hardware settings and begin RX processing task.
						// This entire class will call on_pkt whenever it gets a packet from a
						// blockable thread.

		void send_pkt(const void *packet); // Send a packet out blocking.
		
		void continue_rx(); // Send when done with rx_buf

	protected:
		virtual void on_pkt() = 0; // Called on reception of a valid packet.

		uint8_t rx_buf[257];

	private:
		void rx_task();

		TaskHandle_t rx_thread;
	};
}

#endif
