#include "srv.h"

#include "stm32f2xx.h"
#include "stm32f2xx_ll_usart.h"
#include "stm32f2xx_ll_dma.h"
#include "stm32f2xx_ll_bus.h"

#define STATE_UNINIT 0
#define STATE_HANDSHAKE_RECV 1
#define STATE_HANDSHAKE_SENT 2
#define STATE_DMA_WAIT_SIZE 3
#define STATE_DMA_GOING 4
#define STATE_DMA_READY 5

bool srv::Servicer::important() {
	return this->state == STATE_DMA_READY || this->pending_count > 0;
}

bool srv::Servicer::done() {
	return true; // always runs in one loop iter
}

void srv::Servicer::loop() {

}

bool srv::Servicer::open_slot(uint16_t data_id, bool continuous, uint8_t &slot_id_out, bool persistent) {
	// first, we need to find a slot id
	int slot;
	if (persistent) {
		for (slot = 255; slot >= 0; --slot) {
			if (!this->slot_open(slot)) break;
		}
		if (slot < 0) {
			return false;
		}
	}
	else {
		for (slot = 0; slot < 256; ++slot) {
			if (!this->slot_open(slot)) break;
		}
		if (slot == 256) {
			return false;
		}
	}

	// ok, now we need to check if there's some available space in the pending buffer
	
	if (this->pending_count >= 6) {
		return false;
	}

	this->pending_operations[this->pending_count] = (continuous ? 0x000000 : 0x010000) | data_id;
	++this->pending_count;

	slot_id_out = slot;
	return true;
}

bool srv::Servicer::close_slot(uint8_t slot_id) {
	if (!this->slot_open(slot_id)) return false;

	if (this->pending_count >= 6) {
		return false;
	}

	this->pending_operations[this->pending_count] = 0x100000 | slot_id;
	++this->pending_count;
}

bool srv::Servicer::ack_slot(uint8_t slot_id) {
	if (!this->slot_open(slot_id)) return false;
	if (!this->slot_connected(slot_id)) return false;

	if (this->pending_count >= 6) {
		return false;
	}

	this->pending_operations[this->pending_count] = 0x200000 | slot_id;
	++this->pending_count;
}

inline const uint8_t * srv::Servicer::slot(uint8_t slot_id) {
	return this->slots[slot_id];
}

// packed 2-bit array, with bit 1 in each pair being connected bit 0 being open.

inline bool srv::Servicer::slot_connected(uint8_t i) {
	return (this->slot_states[i / 4] & (1 << (((i % 4) * 2) + 1))) != 0;
}

inline bool srv::Servicer::slot_open(uint8_t i) {
	return (this->slot_states[i / 4] & (1 << (((i % 4) * 2) + 0))) != 0;
}

bool srv::Servicer::ready() {
	return this->state >= STATE_DMA_WAIT_SIZE;
}

void srv::Servicer::init() {
	// Enable clocks

	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
	LL_ABP1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
}
