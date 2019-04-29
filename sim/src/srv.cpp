#include "srv.h"

#include "tasks/timekeeper.h"
#include <string.h>

extern tasks::Timekeeper timekeeper;

#define STATE_UNINIT 0
#define STATE_HANDSHAKE_RECV 1
#define STATE_HANDSHAKE_SENT 2
#define STATE_DMA_WAIT_SIZE 3
#define STATE_DMA_GOING 4

bool srv::Servicer::important() {
	return !done();
}

bool srv::Servicer::done() {
	return true; // always runs in one loop iter
}

void srv::Servicer::loop() {
	if (this->state < STATE_DMA_WAIT_SIZE) {
		// currently doing the handshake
		switch (this->state) {
			case STATE_UNINIT:
				{
					// Send out the HANDSHAKE_INIT command.
					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[1] = 0x00;
					dma_out_buffer[2] = 0x10;
					while (is_sending) {;}
					send();
					start_recv();
					state = STATE_HANDSHAKE_RECV;
					break;
				}
			case STATE_HANDSHAKE_RECV:
				{
					++retry_counter;
					asm volatile ("nop");
					asm volatile ("nop");
					asm volatile ("nop");
					asm volatile ("nop");
					if (retry_counter == 300) {
						state = STATE_HANDSHAKE_SENT; // todoarg
					}
				}
				break;
			case STATE_HANDSHAKE_SENT:
				{
					// Send out the HANDSHAKE_OK command.
					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[1] = 0x00;
					dma_out_buffer[2] = 0x12;
					send();

					start_recv();
					break;
				}
			default:
				break;
		}
	}
	else {
		if (this->pending_count > 0 && !is_sending) {
			uint32_t pending_operation = this->pending_operations[--this->pending_count];
			do_send_operation(pending_operation);
		}

		if ((timekeeper.current_time - last_comm) > 10000 && !sent_ping) {
			this->pending_operations[pending_count++] = 0x51000000;
			sent_ping = true;
		}
		if ((timekeeper.current_time - last_comm) > 25000) {
			// Reset.
		}
	}
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
	
	if (this->pending_count >= 16) {
		return false;
	}

	this->pending_operations[this->pending_count] = (continuous ? 0x00000000 : 0x01000000) | (data_id << 8) | (slot);
	++this->pending_count;

	slot_id_out = slot;
	this->slot_states[slot / 4] |= (1 << (((slot % 4) * 2) + 0)); // mark slot as open, not connected
	this->slot_states[slot / 4] &= ~(1 << (((slot % 4) * 2) + 1)); // mark slot as open, not connected
	return true;
}

bool srv::Servicer::close_slot(uint8_t slot_id) {
	if (!this->slot_open(slot_id)) return false;

	if (this->pending_count >= 16) {
		return false;
	}

	this->pending_operations[this->pending_count] = 0x10000000 | slot_id;
	++this->pending_count;
	return true;
}

bool srv::Servicer::ack_slot(uint8_t slot_id) {
	if (!this->slot_open(slot_id)) return false;
	if (!this->slot_connected(slot_id)) return false;

	if (this->pending_count >= 16) {
		return false;
	}

	this->pending_operations[this->pending_count] = 0x20000000 | slot_id;
	++this->pending_count;
	return true;
}

const uint8_t * srv::Servicer::slot(uint8_t slot_id) {
	return this->slots[slot_id];
}

// packed 2-bit array, with bit 1 in each pair being connected bit 0 being open.

bool srv::Servicer::slot_connected(uint8_t i) {
	return (this->slot_states[i / 4] & (1 << (((i % 4) * 2) + 1))) != 0;
}

bool srv::Servicer::slot_open(uint8_t i) {
	return (this->slot_states[i / 4] & (1 << (((i % 4) * 2) + 0))) != 0;
}

bool srv::Servicer::slot_dirty(uint8_t i, bool mark_clean) {
	if (mark_clean) {
		bool current_val = slot_dirty(i, false);
		this->slot_dirties[i / 8] &= ~(1 << (i % 8));
		return current_val;
	}
	else {
		return (this->slot_dirties[i / 8] & (1 << (i % 8))) != 0;
	}
}

bool srv::Servicer::ready() {
	return this->state >= STATE_DMA_WAIT_SIZE;
}

void srv::Servicer::init() {
	// Set name
	
	name[0] = 's';
	name[1] = 'r';
	name[2] = 'v';
	name[3] = 'c';

	// Enable clock
}

void srv::Servicer::send() {
	// Send whatever is in the dma_out_buffer
	if (is_sending) return;
	is_sending = true;


}

void srv::Servicer::start_recv() {
	state = STATE_DMA_WAIT_SIZE;
	// setup a receieve of 3 bytes
}

void srv::Servicer::recv_full() {
	state = STATE_DMA_GOING;
	// setup a recieve similar to out_buffer
}

void srv::Servicer::do_send_operation(uint32_t operation) {
	// switch on the operation
	uint8_t op = (operation >> 24) & 0xFF;
	uint32_t param = operation & 0xFFFFFF;

	switch (op) {
		case 0x00:
		case 0x01:
			{
				// send an OPEN_CONN command
				bool cont = op == 0x01;
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x04;
				dma_out_buffer[2] = 0x20;

				dma_out_buffer[3] = (cont ? 0x01 : 0x00);
				dma_out_buffer[4] = (param & 0x0000FF);
				dma_out_buffer[5] = (param & 0x00FF00) >> 8;
				dma_out_buffer[6] = (param & 0xFF0000) >> 16;
				send();
			}
			break;
		case 0x10:
			{
				// send a CLOSE_CONN command
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x01;
				dma_out_buffer[2] = 0x21;

				dma_out_buffer[3] = param & 0x0000FF;
				send();
			}
			break;
		case 0x20:
			{
				// send a NEW_DATA command
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x01;
				dma_out_buffer[2] = 0x22;

				dma_out_buffer[3] = param & 0x0000FF;
				send();
			}
			break;
		case 0x50:
			{
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = 0x52;

				send();
			}
			break;
		case 0x51:
			{
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = 0x51;

				send();
			}
			break;

	}
}

void srv::Servicer::process_command() {
	// determine what to do
    if (dma_buffer[0] != 0xa6) {
		// a6 means from the esp
		return;
	}

	switch (dma_buffer[2]) {
		case 0x30:
			{
				// service an acknowledged open command (connected)
				// ACK_OPEN_CONN
				uint8_t slot = dma_buffer[3];

				this->slot_states[slot / 4] |= (1 << (((slot % 4) * 2) + 1)); // mark slot as connected
			}
			break;
		case 0x31:
			{
				// service a ACK_CLOSE_CONN.
				uint8_t slot = dma_buffer[3];

				this->slot_states[slot / 4] &= ~(uint8_t)(0b11 << ((slot % 4)*2)); // mark the slot as unopen and disconnected
			}
			break;
		case 0x40:
			{
				// service a SLOT_DATA command
				//
				// contains d0 = slot_id
				// contains d1-d(size-1) data
				//
				// memset(the array, 0)
				uint8_t slot = dma_buffer[3];
				uint8_t len = dma_buffer[1] - 1;

				memset(slots[slot], 0, 16);
				memcpy(slots[slot], &dma_buffer[4], len);

				slot_dirties[slot / 8] |= (1 << (slot % 8));
			}
			break;
		case 0x50:
			{
				// RESET
			}
			break;
		case 0x51:
			{
				// PING
				if (pending_count == 16) break;
				this->pending_operations[pending_count++] = 0x50000000;
				break;
			}
		default:
			break;
		// TODO: notif
	}

}

void srv::Servicer::dma_finish(bool incoming) {
	if (incoming) {
		last_comm = timekeeper.current_time;
		sent_ping = false;
		// first, check if we need to handle the handshake command
		if (state == STATE_HANDSHAKE_RECV) {
			if (dma_buffer[0] == 0xa6 && dma_buffer[1] == 0x00 && dma_buffer[2] == 0x11) {
				state = STATE_HANDSHAKE_SENT;
				return;
			}
			else {
				state = STATE_UNINIT;
				return;
			}
		}
		else if (state == STATE_DMA_WAIT_SIZE && dma_buffer[1] != 0x00) {
			if (dma_buffer[0] != 0xa6) {
				start_recv();
			}
			recv_full();
		}
		else {
			process_command();

			start_recv();
		}
	}
	else {
		is_sending = false;
	}
}

void srv::Servicer::cancel_recv() {
}
