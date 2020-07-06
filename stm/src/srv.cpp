#include "srv.h"

#include "common/slots.h"
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

constexpr uint16_t srv_reclaim_low_watermark = 2048;
constexpr uint16_t srv_reclaim_high_watermark = 4096;

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

		if (should_reclaim_amt || arena.free_space() < srv_reclaim_low_watermark) {
			should_reclaim_amt = std::max(should_reclaim_amt, srv_reclaim_high_watermark);

			auto reclaim = [&](auto x){
				should_reclaim_amt = (should_reclaim_amt > x) ? should_reclaim_amt - x : 0;
			};
			// Try to defrag
			if (arena.free_space(arena.FreeSpaceDefrag)) {
				uint32_t before = arena.free_space();
				arena.defrag();
				bcache.evict();
				reclaim(before - arena.free_space());
			}
			while (should_reclaim_amt) {
				// evict ephemeral
				for (bheap::Block& b : arena) {
					if (b.temperature == bheap::Block::TemperatureCold && b.location == bheap::Block::LocationEphemeral && b) {
						reclaim(b.datasize + 4);
						bcache.evict(b.slotid);
						b.slotid = bheap::Block::SlotEmpty;
						goto continue_reclaim;
					}
				}
				// evict canonical
				// TODO
				break;
continue_reclaim:
				;
			}

			should_reclaim_amt = 0;
		}
		// send out console information
		else if (debug_out.remaining() && !this->pending_count && !is_sending) {
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

	auto param_hi = [&](){
		return pending_operations[--this->pending_count];
	};

	switch (op) {
		case 0x20:
			// update data temperature
			{
				uint8_t newtemp = param & 0xff;
				uint16_t slotid = param >> 8;

				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 3;
				dma_out_buffer[2] = slots::protocol::DATA_TEMP;
				memcpy(dma_out_buffer + 3, &slotid, 2);
				memcpy(dma_out_buffer + 5, &newtemp, 1);

				send();
			}
			break;
		case 0x21:
		case 0x22:
			// send an ACK to data_move / data_update
			{
				uint32_t slotid_start = param_hi();
				uint16_t slotid = slotid_start & 0xffff;
				uint16_t start =  (slotid_start >> 16) & 0xffff;
				uint16_t length = param >> 8;
				uint8_t  code = param & 0xff;

				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 7;
				dma_out_buffer[2] = op + 0x10;

				memcpy(dma_out_buffer + 3, &slotid, 2);
				memcpy(dma_out_buffer + 5, &start, 2);
				memcpy(dma_out_buffer + 7, &length, 2);
				dma_out_buffer[9] = code;

				send();
			}
			break;
		case 0x23:
			// send an ack to a DATA_DEL
			{
				uint16_t slotid = param & 0xffff;
				
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 2;
				dma_out_buffer[2] = slots::protocol::ACK_DATA_DEL;

				memcpy(dma_out_buffer + 3, &slotid, 2);

				send();
			}
			break;
		case 0x30:
			// send a DATA_MOVE
			{
				uint32_t slotid_start = param_hi();
				uint16_t slotid = slotid_start & 0xffff;
				uint16_t start =  (slotid_start >> 16) & 0xffff;
				uint16_t length = param & 0xff;

				const auto& blk = arena.get(slotid, start);
				if (!blk) return;
				if (arena.block_offset(blk) != start) return; // This is OK to do, since we always send the beginning of a flushable chunk.
				uint8_t buf[length];
				// Send an appropriate data_move
				memcpy(buf, arena.get(slotid, start).data(), length);

				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 8 + length;
				dma_out_buffer[2] = slots::protocol::DATA_MOVE;

				slotid |= 0xC000;

				memcpy(dma_out_buffer + 3, &slotid, 2);
				memcpy(dma_out_buffer + 5, &start, 2);
				// ESP ignores total length when sent it.
				memcpy(dma_out_buffer + 9, &length, 2);
				memcpy(dma_out_buffer + 11, buf, length); // length is limited to 52

				send();
			}
			break;
		case 0x40:
			// Send free heap
			{
				uint16_t fheap = arena.free_space();
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x02;
				dma_out_buffer[2] = slots::protocol::QUERY_FREE_HEAP;
				memcpy(dma_out_buffer + 3, &fheap, 2);

				send();
			}
			break;
		case 0x50:
			{
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = slots::protocol::PONG;

				send();
			}
			break;
		case 0x51:
			{
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = slots::protocol::PING;

				send();
			}
			break;
		// update
		case 0x60:
			{
				// send an update_status
				
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x01;
				dma_out_buffer[2] = slots::protocol::UPDATE_STATUS;
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

	namespace sp = slots::protocol;

	switch (dma_buffer[2]) {
		case sp::QUERY_FREE_HEAP:
			{
				if (pending_count == 32) break;
				this->pending_operations[pending_count++] = 0x40000000;
				break;
			}
			break;
		case sp::DATA_DEL:
			{
				// Immediately truncate to size = 0
				uint16_t slotid = *(uint16_t *)(dma_buffer + 3);
				arena.truncate_contents(slotid, 0);
				if (pending_count == 32) break;
				this->pending_operations[pending_count++] = 0x2300'0000 | slotid;
			}
			break;
		case sp::DATA_UPDATE:
		case sp::DATA_MOVE:
			{
				uint16_t sid_frame = *(uint16_t *)(dma_buffer + 3);
				uint16_t offset = *(uint16_t *)(dma_buffer + 5);
				uint16_t totallen = *(uint16_t *)(dma_buffer + 7);
				uint16_t totalupdlen = *(uint16_t *)(dma_buffer + 9);
				uint8_t  packetlength = dma_buffer[1] - 8;

				bool start = sid_frame & (1 << 15);
				bool end = sid_frame & (1 << 14);
				uint16_t slotid = sid_frame & 0xfff;

				uint8_t errcode = 0;
				uint16_t targetlocation = dma_buffer[2] == slots::protocol::DATA_UPDATE ? bheap::Block::LocationEphemeral : bheap::Block::LocationCanonical;
				
				// We're trying to avoid storing state between messages, so we divide the process into multiple steps:
				if (start) {
					// If this is the starting packet, we ensure there's enough space at the end of the buffer.
					auto currsize = arena.contents_size(slotid);
					if (currsize == arena.npos) currsize = 0;
					if (totallen > 8400) errcode = 0x3;
					else if (currsize < totallen) {
						// Just make a remote chunk
						// Does not invalidate cache
						if (!arena.add_block(slotid, bheap::Block::LocationRemote, totallen - currsize)) errcode = 0x1;
					}
					else if (currsize > totallen) {
						// Truncate
						if (!arena.truncate_contents(slotid, totallen)) errcode = 0x2;
					}

					// Also try to create the actual location type
					if (!errcode) {
						if (!arena.check_location(slotid, offset, totalupdlen, targetlocation)) {
							bcache.evict();
							if (!arena.set_location(slotid, offset, totalupdlen, targetlocation)) errcode = 0x1;
						}
					}
				}
				else {
					// Otherwise just verify it's OK
					if (!arena.check_location(slotid, offset, packetlength, targetlocation)) errcode = 0x1;
				}

				if (errcode) {
do_ack:
					if (!end) break;

					// Only send a response for end == true
					uint16_t origoffset = offset - (totalupdlen - packetlength);

					uint32_t param_hi = static_cast<uint32_t>(slotid) | static_cast<uint32_t>(origoffset) << 16;
					uint32_t operation = dma_buffer[2];
					operation <<= 24;
					operation |= errcode;
					operation |= static_cast<uint32_t>(totalupdlen) << 8;

					if (pending_count > 30) pending_count = 30;
					pending_operations[this->pending_count++] = param_hi;
					pending_operations[this->pending_count++] = operation;

					if (errcode == 0x1) {
						should_reclaim_amt = std::max(should_reclaim_amt, totalupdlen);
					}
					break;
				}

				// Update the data of the slot
				if (!arena.update_contents(slotid, offset, packetlength, dma_buffer + 11)) errcode = 0x02;
				goto do_ack;
			}
			break;
		case sp::ACK_DATA_TEMP:
			{
				// Actually update the temperature
				uint16_t slotid = *(uint16_t *)(dma_buffer + 3);
				uint8_t  temperature = dma_buffer[5];

				arena.set_temperature(slotid, temperature);
			}
			break;
		case sp::ACK_DATA_MOVE:
			{
				uint16_t slotid = *(uint16_t *)(dma_buffer + 3);
				uint16_t updstart = *(uint16_t *)(dma_buffer + 5);
				uint16_t updlen = *(uint16_t *)(dma_buffer + 7);
				uint8_t result = dma_buffer[9]; 
				// Ensure the location makes sense

				if (result) break;
				if (!arena.check_location(slotid, updstart, updlen, bheap::Block::LocationCanonical)) break;

				// Update the length + clear the cache
				bcache.evict();
				arena.set_location(slotid, updstart, updlen, bheap::Block::LocationRemote);
			}
			break;
		case sp::RESET:
			{
				// RESET
				NVIC_SystemReset();
			}
			break;
		case sp::PING:
			{
				// PING
				if (pending_count == 32) break;
				this->pending_operations[pending_count++] = 0x50000000;
				break;
			}
		case sp::UPDATE_CMD:
			{
				// UPDATE_CMD
				process_update_cmd(dma_buffer[3]);
				break;
			}
		case sp::UPDATE_IMG_START: 
			{
				// UPDATE_MSG_START
				
				this->update_chunks_remaining = *(uint16_t *)(dma_buffer + 9);
				this->update_total_size = *(uint32_t *)(dma_buffer + 5);
				this->update_checksum = *(uint16_t *)(dma_buffer + 3);

				// Send the next status
				
				this->pending_operations[pending_count++] = 0x60'0000'13;
				break;
			}
		case sp::UPDATE_IMG_DATA:
			{
				this->update_pkg_size = this->dma_buffer[1] - 2;
				memcpy(this->update_pkg_buffer, this->dma_buffer + 5, this->update_pkg_size);  // copy data
				memcpy(this->update_pkg_buffer + this->update_pkg_size, this->dma_buffer + 3, 2); // copy crc

				append_data(this->update_state, this->update_pkg_buffer, this->update_pkg_size);
				break;
			}
		case sp::CONSOLE_MSG:
			{
				// CONSOLE_MESSAGE
				
				if (this->dma_buffer[3] != 0x01) break; // if console is not the dinput
				
				debug_in.write((char *)this->dma_buffer + 4, this->dma_buffer[1] - 1);
				break;
			}
		default:
			break;
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
