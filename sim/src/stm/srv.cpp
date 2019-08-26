#include "srv.h"

#include "tasks/timekeeper.h"
#include <string.h>
#include "common/bootcmd.h"
#include "common/util.h"
#include "pins.h"
#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>

extern tasks::Timekeeper timekeeper;

#define STATE_UNINIT 0
#define STATE_HANDSHAKE_RECV 1
#define STATE_HANDSHAKE_SENT 2
#define STATE_UPDATE_STARTING 3
#define STATE_DMA_WAIT_SIZE 4
#define STATE_DMA_GOING 5

#define USTATE_WAITING_FOR_READY 0
#define USTATE_SEND_READY 20
#define USTATE_ERASING_BEFORE_IMAGE 1
#define USTATE_WAITING_FOR_PACKET 2
#define USTATE_PACKET_WRITTEN 3
#define USTATE_ERASING_BEFORE_PACKET 4
#define USTATE_FAILED 10
#define USTATE_PACKET_WRITE_FAIL_CSUM 11
// not the dish soap
#define USTATE_WAITING_FOR_FINISH 12

// Updating routines

uint32_t update_package_data_ptr;
uint32_t update_package_data_counter = 0;
uint8_t update_package_sector_counter = 5;

uint32_t faux_dma_counter = 0;
uint8_t * faux_dma_ptr = 0;

void begin_update(uint8_t &state) {
	state = USTATE_FAILED;
}

void append_data(uint8_t &state, uint8_t * data, size_t amt, bool already_erased=false) {
	state = USTATE_FAILED;
}


bool srv::Servicer::important() {
	return !done();
}

bool srv::Servicer::done() {
	return (this->pending_count == 0 && !is_sending); // always runs in one loop iter
}

void srv::Servicer::loop() {
	// handle fauxdma:tm:
	if (!(faux_dma_ptr == 0 || faux_dma_counter == 0)) {
		int bytes_waiting;
		ioctl(0, FIONREAD, &bytes_waiting);
		while (bytes_waiting--) {
			if (faux_dma_ptr == 0) break;
			*faux_dma_ptr++ = getchar();
			if (!--faux_dma_counter) {
				faux_dma_ptr = 0;
				dma_finish(true);
			}
		}
	}

	if (this->state < STATE_DMA_WAIT_SIZE) {
		if (last_comm > 10000) {
			// invalid state error -- attempt going back to the main loop
			start_recv();
		}
		// currently doing the handshake
		switch (this->state) {
			case STATE_UNINIT:
				{
					// Check if the system has just updated
					if (bootcmd_did_just_update()) {
						// We did! Send out an update state finished
						dma_out_buffer[0] = 0xa5;
						dma_out_buffer[1] = 0x01;
						dma_out_buffer[2] = 0x63;
						dma_out_buffer[3] = 0x21;

						update_state = USTATE_WAITING_FOR_FINISH;
						send();
						is_updating = true;
						start_recv();
						break;
					}
					else {
						// Send out the HANDSHAKE_INIT command.
						dma_out_buffer[0] = 0xa5;
						dma_out_buffer[1] = 0x00;
						dma_out_buffer[2] = 0x10;
						send();
						start_recv();
						state = STATE_HANDSHAKE_RECV;
						break;
					}
				}
			case STATE_HANDSHAKE_RECV:
				{
					++retry_counter;
					asm volatile ("nop");
					asm volatile ("nop");
					asm volatile ("nop");
					asm volatile ("nop");
					if (retry_counter == 300) {
						std::exit(1);
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
			case STATE_UPDATE_STARTING:
				{
					// Enter update mode, and tell the ESP we did
					
					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[1] = 0x01;
					dma_out_buffer[2] = 0x63;
					dma_out_buffer[3] = 0x10; // UPDATE_STATUS; cmd=update mode entered

					send();
					is_updating = true; // Mark update mode.
					start_recv(); // Begin recieveing messages
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

		if (!is_updating) {
			// authenticate last_time makes sense
			if (timekeeper.current_time - last_comm > 200000) {
				// the failsafe should have activated, otherwise we've gone madd
				last_comm = timekeeper.current_time; // there was probably a time glitch
				// shit happens
				return;
			}

			if ((timekeeper.current_time - last_comm) > 10000 && !sent_ping) {
				this->pending_operations[pending_count++] = 0x51000000;
				sent_ping = true;
			}
			if ((timekeeper.current_time - last_comm) > 40000) {
				// Reset.
				if (timekeeper.current_time < last_comm) {
					last_comm = timekeeper.current_time;
					return;
				}
				std::exit(1);
			}
		}
	}

	// Update ASYNC logic
	if (this->is_updating) {
		switch (this->update_state) {
			case USTATE_PACKET_WRITE_FAIL_CSUM:
			case USTATE_FAILED:
				{
					this->update_state = USTATE_WAITING_FOR_PACKET;
					this->pending_operations[pending_count++] = 0x60'0000'30;
					break;
				}
			default:
				break;
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
	
	if (this->pending_count >= 32) {
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

	if (this->pending_count >= 32) {
		return false;
	}

	this->pending_operations[this->pending_count] = 0x10000000 | slot_id;
	++this->pending_count;
	return true;
}

bool srv::Servicer::ack_slot(uint8_t slot_id) {
	if (!this->slot_open(slot_id)) return false;
	if (!this->slot_connected(slot_id)) return false;

	if (this->pending_count >= 32) {
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

	// Setup fake UART with pipes
	termios term;
    tcgetattr(0, &term);
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    tcsetattr(0, TCSANOW, &term);
}

void srv::Servicer::send() {
	// Send whatever is in the dma_out_buffer
	std::cerr.write((char *)dma_out_buffer, dma_out_buffer[1] + 3);
	is_sending = false;
}

void srv::Servicer::start_recv() {
	state = STATE_DMA_WAIT_SIZE;
	
	faux_dma_ptr = dma_buffer;
	faux_dma_counter = 3;
}

bool srv::Servicer::recv_full() {
	state = STATE_DMA_GOING;

	faux_dma_ptr = dma_buffer + 3;
	faux_dma_counter = dma_buffer[1];

	return false;
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
		// update
		case 0x60:
			{
				// send an update_status
				
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x01;
				dma_out_buffer[2] = 0x63;
				dma_out_buffer[3] = param & 0xFF;

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

				this->slot_states[slot / 4] &= ~(0b11 << ((slot % 4)*2)); // mark the slot as unopen and disconnected
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
				if (len > 16) len = 16;

				memset(slots[slot], 0, 16);
				memcpy(slots[slot], &dma_buffer[4], len);

				slot_dirties[slot / 8] |= (1 << (slot % 8));
			}
			break;
		case 0x50:
			{
				// RESET
				std::exit(1);
			}
			break;
		case 0x51:
			{
				// PING
				if (pending_count == 32) break;
				this->pending_operations[pending_count++] = 0x50000000;
				break;
			}
		case 0x60:
			{
				// UPDATE_CMD
				process_update_cmd(dma_buffer[3]);
				break;
			}
		case 0x61: 
			{
				// UPDATE_MSG_START
				
				this->update_chunks_remaining = *(uint16_t *)(dma_buffer + 9);
				this->update_total_size = *(uint32_t *)(dma_buffer + 5);
				this->update_checksum = *(uint16_t *)(dma_buffer + 3);

				// Send the next status
				
				this->pending_operations[pending_count++] = 0x60'0000'13;
				break;
			}
		case 0x62:
			{
				this->update_pkg_size = this->dma_buffer[1] - 2;
				memcpy(this->update_pkg_buffer, this->dma_buffer + 5, this->update_pkg_size);  // copy data
				memcpy(this->update_pkg_buffer + this->update_pkg_size, this->dma_buffer + 3, 2); // copy crc

				append_data(this->update_state, this->update_pkg_buffer, this->update_pkg_size);
				break;
			}
		default:
			break;
		// TODO: notif
	}

}

void srv::Servicer::process_update_cmd(uint8_t cmd) {
	switch (cmd) {
		case 0x10:
			{
				// Cancel updates
				std::exit(1);
			}
		case 0x11:
			{
				// Prepare to read byte
				begin_update(this->update_state);

				// When BSY bit is 0 the loop will send us to the right state.
				break;
			}
		case 0x40:
			{
				// update done!
				std::exit(1);
			}
		default:
			{
				update_state = USTATE_FAILED;
			}
			break;
	}
}

void srv::Servicer::dma_finish(bool incoming) {
	if (incoming) {
		// check for error
		last_comm = timekeeper.current_time;
		sent_ping = false;
		// first, check if we need to handle the handshake command
		if (state == STATE_HANDSHAKE_RECV) {
			if (dma_buffer[0] == 0xa6 && dma_buffer[1] == 0x00 && dma_buffer[2] == 0x11) {
				state = STATE_HANDSHAKE_SENT;
				return;
			}
			else if (dma_buffer[0] == 0xa6 && dma_buffer[1] == 0x00 && dma_buffer[2] == 0x13) {
				state = STATE_UPDATE_STARTING;
				return;
			}
			else {
				std::exit(1);
			}
		}
		else if (state == STATE_DMA_WAIT_SIZE && dma_buffer[1] != 0x00) {
			if (dma_buffer[0] != 0xa6) {
				start_recv();
				return;
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

const char * srv::Servicer::update_status() {
	switch (this->update_state) {
		case USTATE_WAITING_FOR_READY:
			snprintf(update_status_buffer, 16, "UPD .. WAIT");
			break;
		case USTATE_ERASING_BEFORE_IMAGE:
			snprintf(update_status_buffer, 16, "UPD .. ESIMG");
			break;
		case USTATE_FAILED:
			snprintf(update_status_buffer, 16, "UPD .. FAIL");
			break;
		case USTATE_PACKET_WRITE_FAIL_CSUM:
			snprintf(update_status_buffer, 16, "UPD .. CFAIL");
			break;
		case USTATE_ERASING_BEFORE_PACKET:
			snprintf(update_status_buffer, 16, "UPD .. EPS%02d", update_package_sector_counter);
			break;
		case USTATE_WAITING_FOR_PACKET:
			snprintf(update_status_buffer, 16, "UPD .. P%04d", update_chunks_remaining);
			break;
		case USTATE_WAITING_FOR_FINISH:
			snprintf(update_status_buffer, 16, "UPD .. WESP");
		default:
			break;
	}

	return update_status_buffer;
}
