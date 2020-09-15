#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>

namespace srv {
	enum struct ProtocolState {
		UNINIT,
		HANDSHAKE_RECV,
		HANDSHAKE_SENT,
		UPDATE_STARTING,
		DMA_WAIT_SIZE,
		DMA_GOING
	};

	// Actually does the UART comms with the ESP
	struct ProtocolImpl {
		// Buffers for DMA

		uint8_t dma_buffer[258];
		uint8_t dma_out_buffer[67];

		// Implementation
		void setup_uart_dma();
		void send();
		void start_recv(srv::ProtocolState target_state=ProtocolState::DMA_WAIT_SIZE);
		void recv_full();

		// State
		bool is_sending = false;
		ProtocolState state = ProtocolState::UNINIT;
		uint64_t last_comm = 0;
		bool sent_ping = false;
		TaskHandle_t notify_on_send_done = nullptr;

		// Interrupt
		void dma_finish(bool incoming); // returns whether or not we should call process_command

	protected:
		virtual void process_command() = 0;
	};
}

#endif
