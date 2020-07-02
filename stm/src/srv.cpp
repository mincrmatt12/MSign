#include "srv.h"

#include "tasks/timekeeper.h"
#include <string.h>
#include "stm32f2xx_ll_usart.h"
#include "stm32f2xx_ll_system.h"
#include "common/bootcmd.h"
#include "common/util.h"
#include "pins.h"
#include "nvic.h"
#include <algorithm>
#include <unistd.h>
#include <stdio.h>

extern tasks::Timekeeper timekeeper;

#define USTATE_WAITING_FOR_READY 0
#define USTATE_SEND_READY 20
#define USTATE_WAITING_FOR_FINISH_SECTORCOUNT 21
#define USTATE_WAITING_FOR_FINISH_WRITE 22
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

void begin_update(uint8_t &state) {
	// Set data_ptr to the beginning of the update memory area.
	
	update_package_data_ptr = 0x0808'0000;
	update_package_sector_counter = 8; // sector 8 is currently being written into.
	update_package_data_counter = 0;

	// Unlock FLASH
	
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;

	// Erase sector 8
	
	CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
	FLASH->CR |= FLASH_CR_SER /* section erase */ | (update_package_sector_counter << FLASH_CR_SNB_Pos);

	// Actually erase
	
	FLASH->CR |= FLASH_CR_STRT;

	state = USTATE_ERASING_BEFORE_IMAGE;
}

void append_data(uint8_t &state, uint8_t * data, size_t amt, bool already_erased=false) {
	if (!util::crc_valid(data, amt + 2)) {
		state = USTATE_PACKET_WRITE_FAIL_CSUM;
		return;
	}

	// Append data to counter
	if ((update_package_data_counter + amt) >= 131072 && !already_erased) {
		++update_package_sector_counter;

		// Erase the sector

		CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
		FLASH->CR |= FLASH_CR_SER /* section erase */ | (update_package_sector_counter << FLASH_CR_SNB_Pos);

		// Actually erase
		
		FLASH->CR |= FLASH_CR_STRT;

		state = USTATE_ERASING_BEFORE_PACKET;

		return;
	}
	else if (already_erased) {
		// we've just erased, so set the data counter to the amount in this new section
		update_package_data_counter = (update_package_data_counter + amt) - 131072;
	}
	else {
		// Otherwise, just program the data
		update_package_data_counter += amt;
	}

	CLEAR_BIT(FLASH->CR, FLASH_CR_PSIZE); // set psize to 0; byte by byte access

	while (amt--) {
		FLASH->CR |= FLASH_CR_PG;
		(*(uint8_t *)(update_package_data_ptr++)) = *(data++); // Program this byte

		// Wait for busy
		while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
			asm volatile ("nop");
			asm volatile ("nop");
			asm volatile ("nop");
			asm volatile ("nop");
		}

		// Program next byte
	}

	// Clear pg byte
	
	CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

	state = USTATE_PACKET_WRITTEN;
}


bool srv::Servicer::important() {
	return !done();
}

bool srv::Servicer::done() {
	return (this->pending_count == 0 && !is_sending); // always runs in one loop iter
}

void srv::Servicer::loop() {
	if (this->state < ProtocolState::DMA_WAIT_SIZE) {
		if (last_comm > 10000) {
			// invalid state error -- attempt going back to the main loop
			start_recv();
		}
		// currently doing the handshake
		switch (this->state) {
			case ProtocolState::UNINIT:
				{
					// Check if the system has just updated
					if (bootcmd_did_just_update()) {
						// We did! Send out an update state finished
						dma_out_buffer[0] = 0xa5;
						dma_out_buffer[1] = 0x01;
						dma_out_buffer[2] = 0x63;
						dma_out_buffer[3] = 0x21;

						update_state = USTATE_WAITING_FOR_FINISH;
						update_chunks_remaining = 0; // used separately
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
						state = ProtocolState::HANDSHAKE_RECV;
						break;
					}
				}
			case ProtocolState::HANDSHAKE_RECV:
				{
					++retry_counter;
					asm volatile ("nop");
					asm volatile ("nop");
					asm volatile ("nop");
					asm volatile ("nop");
					if (retry_counter == 140) {
						NVIC_SystemReset();
					}
				}
				break;
			case ProtocolState::HANDSHAKE_SENT:
				{
					// Send out the HANDSHAKE_OK command.
					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[1] = 0x00;
					dma_out_buffer[2] = 0x12;
					send();


					// clear the overrun flag
					LL_USART_ClearFlag_ORE(ESP_USART);
					LL_USART_ClearFlag_PE(ESP_USART);
					start_recv();
					break;
				}
			case ProtocolState::UPDATE_STARTING:
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
			if ((timekeeper.current_time - last_comm) > 30000) {
				// Reset.
				if (timekeeper.current_time < last_comm) {
					last_comm = timekeeper.current_time;
					return;
				}

				nvic::show_error_screen("ESPTimeout");
			}
		}

		// send out console information
		if (debug_out.remaining() && !this->pending_count && !is_sending) {
			char buf[64];
			uint8_t amt = std::min(debug_out.remaining(), (size_t)64);
			debug_out.read(buf, amt);

			this->dma_out_buffer[0] = 0xa5;
			this->dma_out_buffer[1] = amt + 1;
			this->dma_out_buffer[2] = 0x70;
			this->dma_out_buffer[3] = 0x02;

			memcpy(&dma_out_buffer[4], buf, amt);

			send();
		}
		else if (log_out.remaining() && !this->pending_count && !is_sending) {
			char buf[64];
			uint8_t amt = std::min(log_out.remaining(), (size_t)64);
			log_out.read(buf, amt);

			this->dma_out_buffer[0] = 0xa5;
			this->dma_out_buffer[1] = amt + 1;
			this->dma_out_buffer[2] = 0x70;
			this->dma_out_buffer[3] = 0x10;

			memcpy(&dma_out_buffer[4], buf, amt);

			send();
		}
	}

	// Update ASYNC logic
	if (this->is_updating) {
		switch (this->update_state) {
			case USTATE_ERASING_BEFORE_IMAGE:
				{
					if (!READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
						CLEAR_BIT(FLASH->CR, FLASH_CR_SER);
						// Report readiness for bytes
						this->update_state = USTATE_WAITING_FOR_PACKET;
						this->pending_operations[pending_count++] = 0x60'0000'12;
					}
					break;
				}
			case USTATE_ERASING_BEFORE_PACKET:
				{
					if (!READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
						CLEAR_BIT(FLASH->CR, FLASH_CR_SER);
						this->update_state = USTATE_WAITING_FOR_PACKET;

						append_data(this->update_state, this->update_pkg_buffer, this->update_pkg_size, true);
					}
					break;
				}
			case USTATE_PACKET_WRITTEN:
				{
					if (--this->update_chunks_remaining == 0) {
						// We are done?
						//
						// Verify checksum
					
						// Disable D-Cache and flush it
						LL_FLASH_DisableDataCache();
						LL_FLASH_EnableDataCacheReset();
						LL_FLASH_DisableDataCacheReset();

						if (util::compute_crc((uint8_t *)(0x0808'0000), this->update_total_size) != this->update_checksum) {
							this->update_state = USTATE_FAILED;
							this->pending_operations[pending_count++] = 0x60'0000'40;

							break;
						}

						// Checksum is OK, write the BCMD
						
						bootcmd_request_update(this->update_total_size);

						// Clear pg byte
						
						CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

						// Relock FLASH
						
						FLASH->CR |= FLASH_CR_LOCK;

						// Send out final message
						
						do_send_operation(0x60'0000'20);

						// Wait a bit for DMA
						
						for (int i = 0; i < 400000; ++i) {
							asm volatile ("nop");
						}

						// Reset
						
						NVIC_SystemReset();

						// At this point the bootloader will update us.
					}

					this->update_state = USTATE_WAITING_FOR_PACKET;
					this->pending_operations[pending_count++] = 0x60'0000'13;
					break;
				}
			case USTATE_PACKET_WRITE_FAIL_CSUM:
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
	if (!this->slot_connected(slot_id)) return false;

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
	return this->state >= ProtocolState::DMA_WAIT_SIZE;
}

void srv::Servicer::init() {
	// Set name
	
	name[0] = 's';
	name[1] = 'r';
	name[2] = 'v';
	name[3] = 'c';

	// Setup HW
	this->setup_uart_dma();
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
				NVIC_SystemReset();
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
		case 0x70:
			{
				// CONSOLE_MESSAGE
				
				if (this->dma_buffer[3] != 0x01) break; // if console is not the dinput
				
				debug_in.write((char *)this->dma_buffer + 4, this->dma_buffer[1] - 1);
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
				NVIC_SystemReset();
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
				NVIC_SystemReset();
			}
			break;
		case 0x50:
			{
				// sector count up
				++update_chunks_remaining;
				this->update_state = USTATE_WAITING_FOR_FINISH_SECTORCOUNT;
				break;
			}
		case 0x51:
			{
				// writing
				this->update_state = USTATE_WAITING_FOR_FINISH_WRITE;
				break;
			}
		default:
			{
				update_state = USTATE_FAILED;
			}
			break;
	}
}

const char * srv::Servicer::update_status() {
	switch (this->update_state) {
		case USTATE_WAITING_FOR_READY:
			snprintf(update_status_buffer, 16, "UPD WAIT");
			break;
		case USTATE_ERASING_BEFORE_IMAGE:
			snprintf(update_status_buffer, 16, "UPD ESIMG");
			break;
		case USTATE_FAILED:
			snprintf(update_status_buffer, 16, "UPD FAIL");
			break;
		case USTATE_PACKET_WRITE_FAIL_CSUM:
			snprintf(update_status_buffer, 16, "UPD CFAIL");
			break;
		case USTATE_ERASING_BEFORE_PACKET:
			snprintf(update_status_buffer, 16, "UPD EPS%02d", update_package_sector_counter);
			break;
		case USTATE_WAITING_FOR_PACKET:
			snprintf(update_status_buffer, 16, "UPD P%04d", update_chunks_remaining);
			break;
		case USTATE_WAITING_FOR_FINISH:
			snprintf(update_status_buffer, 16, "UPD WESP");
			break;
		case USTATE_WAITING_FOR_FINISH_SECTORCOUNT:
			snprintf(update_status_buffer, 16, "UPD ESCT%04d", update_chunks_remaining);
			break;
		case USTATE_WAITING_FOR_FINISH_WRITE:
			snprintf(update_status_buffer, 16, "UPD EWRT");
		default:
			break;
	}

	return update_status_buffer;
}

// ConIO interface

size_t srv::ConIO::remaining() {
	if (start <= end) return end - start;
	else return (&buf[512] - start) + (end - &buf[0]);
}

void srv::ConIO::write(const char * ibuf, size_t length) {
	if (remaining() + length > 512) return;

	memcpy(end, ibuf, std::min((ptrdiff_t)length, &buf[512] - end));
	if (length > &buf[512] - end) {
		memcpy(buf, ibuf + (&buf[512] - end), length - (&buf[512] - end));
		end = &buf[length - (&buf[512] - end)];
	}
	else {
		end += length;
	}
}

void srv::ConIO::read(char * obuf, size_t length) {
	if (length > remaining()) return;

	memcpy(obuf, start, std::min((ptrdiff_t)length, &buf[512] - start));
	if (length > &buf[512] - start) {
		memcpy(obuf + (&buf[512] - start), buf, length - (&buf[512] - start));
		start = &buf[length - (&buf[512] - start)];
	}
	else {
		start += length;
	}
}

srv::ConIO debug_in, debug_out;
srv::ConIO log_out;

// override _write

extern "C" int _write(int file, char* ptr, int len) {
	if (file == STDOUT_FILENO) log_out.write(ptr, len);
	else                       debug_out.write(ptr, len);
	return len;
}
