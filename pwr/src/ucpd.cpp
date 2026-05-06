#include "ucpd.h"
#include "regman.h"

#include <stm32g0xx_ll_ucpd.h>
#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_system.h>
#include <stm32g0xx_ll_dma.h>
#include <stm32g0xx_ll_dmamux.h>

#include <string.h>

namespace ucpd {
	void strobe_preinit() {
		{
			LL_UCPD_InitTypeDef init {
				.psc_ucpdclk = LL_UCPD_PSC_DIV2, // ucpd clk is 16MHz HSI, div / 2 = 8MHz (which is in recommended 6-8mhz range)
				.transwin = 7, // divide by 8   - stolen from the LL struct init
				.IfrGap = 16,  // divide by 17
				.HbitClockDiv = 13, // divide by 14, just under 600khz
			};

			LL_UCPD_Init(UCPD1, &init);
		}
		
		LL_UCPD_SetRxOrderSet(UCPD1, LL_UCPD_ORDERSET_SOP | LL_UCPD_ORDERSET_HARDRST);
		UCPD1->CFG1 |= UCPD_CFG1_RXDMAEN | UCPD_CFG1_TXDMAEN;

		LL_UCPD_Enable(UCPD1);

		LL_UCPD_SetSNKRole(UCPD1);
		LL_UCPD_SetccEnable(UCPD1, LL_UCPD_CCENABLE_CC1CC2);

		LL_UCPD_TypeCDetectionCC1Enable(UCPD1);
		LL_UCPD_TypeCDetectionCC2Enable(UCPD1);

		SYSCFG->CFGR1 |= SYSCFG_CFGR1_UCPD1_STROBE;

		LL_SYSCFG_EnablePinRemap(LL_SYSCFG_PIN_RMP_PA11 | LL_SYSCFG_PIN_RMP_PA12);
	}

	PortManager::PortManager() {
		// setup dma for reception

		LL_DMA_InitTypeDef init {
			.PeriphOrM2MSrcAddress = (uint32_t)&UCPD1->RXDR,
			.MemoryOrM2MDstAddress = (uint32_t)&dma_rx_buf[0],
			.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
			.Mode = LL_DMA_MODE_NORMAL,
			.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT,
			.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT,
			.PeriphOrM2MSrcDataSize = LL_DMA_MDATAALIGN_BYTE,
			.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE,
			.NbData = sizeof dma_rx_buf,
			.PeriphRequest = LL_DMAMUX_REQ_UCPD1_RX,
			.Priority = LL_DMA_PRIORITY_HIGH
		};

		LL_DMA_Init(DMA1, LL_DMA_CHANNEL_4, &init);

		init = {
			.PeriphOrM2MSrcAddress = (uint32_t)&UCPD1->TXDR,
			.MemoryOrM2MDstAddress = (uint32_t)&dma_tx_buf[0],
			.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
			.Mode = LL_DMA_MODE_NORMAL,
			.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT,
			.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT,
			.PeriphOrM2MSrcDataSize = LL_DMA_MDATAALIGN_BYTE,
			.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE,
			.NbData = sizeof dma_rx_buf,
			.PeriphRequest = LL_DMAMUX_REQ_UCPD1_TX,
			.Priority = LL_DMA_PRIORITY_HIGH
		};

		LL_DMA_Init(DMA1, LL_DMA_CHANNEL_5, &init);

		// configure our interrupt handler

		// prepare for type-c detection state machine
		UCPD1->IMR = UCPD_IMR_TYPECEVT1IE | UCPD_IMR_TYPECEVT2IE;

		NVIC_SetPriority(UCPD1_2_IRQn, 1);
		NVIC_EnableIRQ(UCPD1_2_IRQn);
	}

	bool PortManager::push_isr_req() {
		if (!isr_any) {
			return false;
		}

		BaseType_t woken = pdFALSE;

		if (xQueueIsQueueEmptyFromISR(reqs)) {
			Req r {
				.kind = r.ISR
			};

			xQueueSendToFrontFromISR(reqs, &r, &woken);
		}

		return woken;
	}

	bool PortManager::ucpd_isr() {
		auto flags = UCPD1->SR & (
			UCPD_SR_TYPECEVT1 | UCPD_SR_TYPECEVT2 | UCPD_IMR_RXORDDETIE | UCPD_IMR_RXMSGENDIE | UCPD_IMR_TXMSGDISCIE | UCPD_IMR_TXMSGSENTIE
		);
		UCPD1->ICR = flags;

		bool woken = false;

		if (flags & (UCPD_SR_TYPECEVT1 | UCPD_SR_TYPECEVT2)) {
			isr_cable_tick = true;
		}

		if (flags & UCPD_SR_RXORDDET) {
			if (trans_state == TxRxIdle) {
				trans_state = RxPktIncoming;
			}
			else {
				// todo: panic
			}
		}
		
		if (flags & UCPD_SR_RXMSGEND) {
			if (trans_state == RxPktIncoming) {
				trans_state = RxPktReady;
				isr_msg_rx = true;
				last_dma_rx_size = UCPD1->RX_PAYSZ;

				LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
			}
			else {
				// todo: discarded
			}
		}

		if (flags & UCPD_SR_TXMSGSENT) {
			if (trans_state == RxSendingCrc) {
				trans_state = TxRxIdle;
				isr_goodcrc = true;
			}
			else if (trans_state == TxSendingPkt) {
				trans_state = TxRxIdle;
				isr_sent = true;
			}
		}

		return push_isr_req();
	}

	void PortManager::tick_cable() {
		if (cable_is_present) {
			return;
		}

		uint32_t sr = UCPD1->SR;
		uint32_t cc1 = (sr >> UCPD_SR_TYPEC_VSTATE_CC1_Pos) & 0b11;
		uint32_t cc2 = (sr >> UCPD_SR_TYPEC_VSTATE_CC2_Pos) & 0b11;

		if (cc1 && cc2) {
			// debug accessory; ignore
		}
		else if (!cc1 && !cc2) {
			return; // cable not present
		}

		if (cc1) {
			contract_mode = static_cast<Contract>(cc1);
			cc_is_cc2 = false;
		}
		else {
			contract_mode = static_cast<Contract>(cc2);
			cc_is_cc2 = true;
		}

		setup_after_attach();
	}

	void PortManager::reset_rx_dma() {
		if (LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_4)) {
			LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
		}
		while (LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_4));

		DMA1_Channel4->CMAR = (uint32_t)&dma_rx_buf[0];
		DMA1_Channel4->CNDTR = sizeof dma_rx_buf;

		LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
	}

	void PortManager::setup_after_attach() {
		if (cable_is_present)
			return;

		cable_is_present = true;

		MODIFY_REG(UCPD1->CR, UCPD_CR_CCENABLE | UCPD_CR_PHYCCSEL, cc_is_cc2 ? (UCPD_CR_CCENABLE_1 | UCPD_CR_PHYCCSEL) : UCPD_CR_CCENABLE_0);
		UCPD1->IMR |= UCPD_IMR_RXORDDETIE | UCPD_IMR_RXMSGENDIE | UCPD_IMR_TXMSGDISCIE | UCPD_IMR_TXMSGSENTIE;

		reset_rx_dma();
		LL_UCPD_RxEnable(UCPD1);
	}

	// control msgs
	constexpr inline static uint16_t PD_MSG_GOODCRC = 0b00001;
	constexpr inline static uint16_t PD_MSG_PS_RDY = 0b00110;

	// data msgs
	constexpr inline static uint16_t PD_MSG_SOURCE_CAPABILITIES = 0b00001;
	constexpr inline static uint16_t PD_MSG_REQUEST = 0b00010;

	struct PdMsgHeader {
		uint16_t data;

		constexpr explicit PdMsgHeader(uint8_t *buf) {
			data = buf[0] | (buf[1] << 8);
		}

		constexpr PdMsgHeader(
			uint16_t msg_type,
			uint16_t n_data_objects,
			uint16_t msg_id
		) : data((msg_type & 0b11111) | ((n_data_objects & 0b111) << 12) | ((msg_id & 0b111) << 9) | (0b10 << 6))
		{}

		constexpr uint16_t msg_type() const { return data & 0b11111; }
		constexpr uint16_t n_data_objects() const { return (data >> 12) & 0b111; }
		constexpr uint16_t msg_id() const { return (data >> 9) & 0b111; }
	};

	void PortManager::send_goodcrc() {
		PdMsgHeader in_hdr = PdMsgHeader(dma_rx_buf);

		if (in_hdr.msg_type() == PD_MSG_GOODCRC && in_hdr.n_data_objects() == 0) {
			trans_state = TxRxIdle;
			reset_rx_dma();

			return;
		}

		PdMsgHeader hdr = {PD_MSG_GOODCRC, 0, in_hdr.msg_id()};

		memcpy(dma_tx_buf, &hdr, sizeof hdr);
		trans_state = RxSendingCrc;

		blast_tx_dma(2);
	}

	void PortManager::process_msg() {
		auto hdr = PdMsgHeader(dma_rx_buf);

		if (hdr.msg_type() == PD_MSG_SOURCE_CAPABILITIES && hdr.n_data_objects() == 6) {
			// first transmitted sink data message, msgid = 0 is fine for now
			PdMsgHeader hdr = {PD_MSG_REQUEST, 1, 0};

			// PDO4 request for 15V @ 3A
			uint32_t rdo = 0x4004B000;

			memcpy(&dma_tx_buf[0], &hdr, sizeof hdr);
			memcpy(&dma_tx_buf[2], &rdo, sizeof rdo);

			trans_state = TxSendingPkt;
			blast_tx_dma(6);
		}

		if (hdr.msg_type() == PD_MSG_PS_RDY && hdr.n_data_objects() == 0) {
			regman::man->switch_output_mux(regman::MuxController::Regulated);
		}

		reset_rx_dma();
	}

	void PortManager::blast_tx_dma(size_t length) {
		LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_5);
		while (LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_5));

		LL_UCPD_WriteTxOrderSet(UCPD1, LL_UCPD_ORDERED_SET_SOP);
		LL_UCPD_SetTxMode(UCPD1, LL_UCPD_TXMODE_NORMAL);

		DMA1_Channel5->CNDTR = length;
		DMA1_Channel5->CMAR = (uint32_t)&dma_tx_buf[0];

		LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_5);
		LL_UCPD_WriteTxPaySize(UCPD1, length);
		LL_UCPD_SendMessage(UCPD1);
	}

	void PortManager::run() {
		Req pr = {
			.kind = Req::ISR
		};

		while (true) {
			if (isr_any) {
				if (isr_cable_tick)
					tick_cable();

				if (isr_msg_rx)
					send_goodcrc();

				if (isr_goodcrc)
					process_msg();

				isr_any = 0;
			}

			switch (pr.kind) {
				case Req::ISR:
					break;
			}

			if (!xQueueReceive(reqs, &pr, portMAX_DELAY)) {
				pr.kind = Req::ISR;
			}
		}
	}

	extern "C" void UCPD1_2_IRQHandler() {
		portYIELD_FROM_ISR(port->ucpd_isr());
	}

	util::manual_box<PortManager> port;
}
