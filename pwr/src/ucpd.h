#pragma once

#include "manual_box.h"
#include <stddef.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <queue.h>

namespace ucpd {
	// runs as part of initializing power manager; sets Rd on the cc lines and disables pulldowns so
	// the usart init code can remap pa9/pa10
	//
	// incidentically, this also enables the ucpd bus
	void strobe_preinit();

	extern "C" void UCPD1_2_IRQHandler();

	struct PortManager {
		PortManager();

		void run();
	private:
		bool cable_is_present = false;
		bool cc_is_cc2 = false;

		uint8_t dma_rx_buf[264] {};
		size_t last_dma_rx_size = 0;

		void reset_rx_dma();
		void blast_tx_dma(size_t length);

		enum TransState : uint8_t {
			TxRxIdle,
			RxPktIncoming,
			RxPktReady,
			RxSendingCrc,
			TxSendingPkt,
		} trans_state = TxRxIdle;

		uint8_t dma_tx_buf[264] {};

		void process_msg();
		void send_goodcrc();
		void tick_cable();
		void setup_after_attach();

		struct Req {
			enum : uint8_t {
				ISR,
			} kind;
		};

		StaticQueue_t reqs_priv;
		uint8_t reqs_priv_dat[sizeof(Req) * 32];
		QueueHandle_t reqs = xQueueCreateStatic(32, sizeof(Req), reqs_priv_dat, &reqs_priv);

		bool push_isr_req();

		union {
			struct {
				uint32_t isr_cable_tick : 1;
				uint32_t isr_msg_rx : 1;
				uint32_t isr_goodcrc : 1;
				uint32_t isr_sent : 1;
			};
			uint32_t isr_any = 0;
		};

		enum Contract : uint8_t {
			ContractDefault = 0,
			ContractDefault5VUsb = 0b01,
			ContractDefault5V1A5 = 0b10,
			ContractDefault5V3A  = 0b11,
		} contract_mode = ContractDefault;

		friend void UCPD1_2_IRQHandler();
		bool ucpd_isr();
	};

	extern util::manual_box<PortManager> port;
}
