#include "srv.h"

#include "stm32f2xx_ll_system.h"
#include "tasks/timekeeper.h"
#include <string.h>
#include "stm32f2xx_ll_bus.h"
#include "stm32f2xx_ll_usart.h"
#include "stm32f2xx_ll_gpio.h"
#include "stm32f2xx_ll_dma.h"
#include "stm32f2xx.h"
#include "common/bootcmd.h"
#include "common/util.h"
#include "pins.h"
#include "nvic.h"
#include <algorithm>
#include <unistd.h>

extern tasks::Timekeeper timekeeper;

#define STATE_UNINIT 0
#define STATE_HANDSHAKE_RECV 1
#define STATE_HANDSHAKE_SENT 2
#define STATE_UPDATE_STARTING 3
#define STATE_DMA_WAIT_SIZE 4
#define STATE_DMA_GOING 5

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
					if (retry_counter == 140) {
						NVIC_SystemReset();
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


					// clear the overrun flag
					LL_USART_ClearFlag_ORE(ESP_USART);
					LL_USART_ClearFlag_PE(ESP_USART);
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
	return this->state >= STATE_DMA_WAIT_SIZE;
}

void srv::Servicer::init() {
	// Set name
	
	name[0] = 's';
	name[1] = 'r';
	name[2] = 'v';
	name[3] = 'c';

	// Enable clocks

	LL_AHB1_GRP1_EnableClock(UART_DMA_PERIPH | UART_GPIO_PERIPH);
	UART_ENABLE_CALL(UART_UART_PERIPH);

	// Setup USART
	
	LL_USART_InitTypeDef usart_init = {0};
	usart_init.BaudRate = 115200;
	usart_init.OverSampling = LL_USART_OVERSAMPLING_16;
	usart_init.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	usart_init.DataWidth = LL_USART_DATAWIDTH_9B;
	usart_init.Parity = LL_USART_PARITY_EVEN;
	usart_init.StopBits = LL_USART_STOPBITS_1;
	usart_init.TransferDirection = LL_USART_DIRECTION_TX_RX;
	
	LL_USART_Init(ESP_USART, &usart_init);

	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_USART_EnableDMAReq_TX(ESP_USART);

	LL_USART_ConfigAsyncMode(ESP_USART);
	LL_USART_Enable(ESP_USART);

	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_USART_EnableDMAReq_TX(ESP_USART);

	// Setup DMA channels

	LL_DMA_SetChannelSelection(UART_DMA, UART_DMA_RX_Stream, UART_DMA_Channel); // RX
	LL_DMA_SetChannelSelection(UART_DMA, UART_DMA_TX_Stream, UART_DMA_Channel); // TX

	// Config addresses doesn't actually set this.. it just reads it.

	LL_DMA_SetDataTransferDirection(UART_DMA, UART_DMA_RX_Stream, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
	LL_DMA_SetDataTransferDirection(UART_DMA, UART_DMA_TX_Stream, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
	
	LL_DMA_SetStreamPriorityLevel(UART_DMA, UART_DMA_RX_Stream, LL_DMA_PRIORITY_HIGH);
	LL_DMA_SetStreamPriorityLevel(UART_DMA, UART_DMA_TX_Stream, LL_DMA_PRIORITY_MEDIUM);

	LL_DMA_SetMode(UART_DMA, UART_DMA_RX_Stream, LL_DMA_MODE_NORMAL);
	LL_DMA_SetMode(UART_DMA, UART_DMA_TX_Stream, LL_DMA_MODE_NORMAL);

	LL_DMA_SetPeriphIncMode(UART_DMA, UART_DMA_RX_Stream, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMemoryIncMode(UART_DMA, UART_DMA_RX_Stream, LL_DMA_MEMORY_INCREMENT);
	LL_DMA_SetPeriphIncMode(UART_DMA, UART_DMA_TX_Stream, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMemoryIncMode(UART_DMA, UART_DMA_TX_Stream, LL_DMA_MEMORY_INCREMENT);

	LL_DMA_SetPeriphSize(UART_DMA, UART_DMA_RX_Stream, LL_DMA_PDATAALIGN_BYTE);
	LL_DMA_SetMemorySize(UART_DMA, UART_DMA_RX_Stream, LL_DMA_MDATAALIGN_BYTE);
	LL_DMA_SetPeriphSize(UART_DMA, UART_DMA_TX_Stream, LL_DMA_PDATAALIGN_BYTE);
	LL_DMA_SetMemorySize(UART_DMA, UART_DMA_TX_Stream, LL_DMA_MDATAALIGN_BYTE);

	LL_DMA_DisableFifoMode(UART_DMA, UART_DMA_RX_Stream);
	LL_DMA_DisableFifoMode(UART_DMA, UART_DMA_TX_Stream);

	// Setup GPIO
	
	LL_GPIO_InitTypeDef gpio_init = {0};

	gpio_init.Alternate = UART_Af;
	gpio_init.Pin = ESP_USART_TX | ESP_USART_RX;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	gpio_init.Pull = LL_GPIO_PULL_UP;

	LL_GPIO_Init(ESP_USART_Port, &gpio_init);
}

void srv::Servicer::send() {
	// Send whatever is in the dma_out_buffer
	if (is_sending) return;
	is_sending = true;

	LL_DMA_ConfigAddresses(UART_DMA, UART_DMA_TX_Stream, 
			(uint32_t)(dma_out_buffer),
			LL_USART_DMA_GetRegAddr(ESP_USART),
			LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
	LL_DMA_SetDataLength(UART_DMA, UART_DMA_TX_Stream, dma_out_buffer[1] + 3);

	LL_DMA_DisableIT_HT(UART_DMA, UART_DMA_TX_Stream);
	LL_DMA_EnableIT_TC(UART_DMA, UART_DMA_TX_Stream);
	
	LL_USART_Enable(ESP_USART);
	LL_USART_ClearFlag_TC(ESP_USART);
	LL_DMA_EnableStream(UART_DMA, UART_DMA_TX_Stream);

	LL_USART_EnableDMAReq_TX(ESP_USART);
}

void srv::Servicer::start_recv() {
	state = STATE_DMA_WAIT_SIZE;
	// setup a receieve of 3 bytes

	LL_DMA_ConfigAddresses(UART_DMA, UART_DMA_RX_Stream, 
			LL_USART_DMA_GetRegAddr(ESP_USART),
			(uint32_t)(this->dma_buffer),
			LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

	LL_DMA_SetDataLength(UART_DMA, UART_DMA_RX_Stream, 3);

	LL_DMA_EnableIT_TC(UART_DMA, UART_DMA_RX_Stream);
	LL_DMA_EnableIT_TE(UART_DMA, UART_DMA_RX_Stream);

	LL_USART_ClearFlag_RXNE(ESP_USART);
	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_DMA_EnableStream(UART_DMA, UART_DMA_RX_Stream);
}

bool srv::Servicer::recv_full() {
	state = STATE_DMA_GOING;
	// setup a recieve similar to out_buffer

	LL_DMA_ConfigAddresses(UART_DMA, UART_DMA_RX_Stream, 
			LL_USART_DMA_GetRegAddr(ESP_USART),
			(uint32_t)(&this->dma_buffer[3]),
			LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

	LL_DMA_SetDataLength(UART_DMA, UART_DMA_RX_Stream, dma_buffer[1]);

	LL_DMA_EnableIT_TC(UART_DMA, UART_DMA_RX_Stream);
	LL_DMA_EnableIT_TE(UART_DMA, UART_DMA_RX_Stream);

	LL_USART_ClearFlag_RXNE(ESP_USART);
	LL_USART_EnableDMAReq_RX(ESP_USART);
	LL_DMA_EnableStream(UART_DMA, UART_DMA_RX_Stream);
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

void srv::Servicer::dma_finish(bool incoming) {
	if (incoming) {
		// check for error
		if (NVIC_SRV_RXE_ACTV(UART_DMA)) {
			NVIC_SRV_RXE_CLRF(UART_DMA);
			LL_DMA_DisableStream(UART_DMA, UART_DMA_RX_Stream);

			// this is a deceptive command, clearing flags is done by reading SR and DR, which this function does.
			// it also "reads" a byte off of the serial port but eh.
			LL_USART_ClearFlag_ORE(ESP_USART);

			start_recv();
			return;
		}

		LL_DMA_DisableStream(UART_DMA, UART_DMA_RX_Stream);

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
				NVIC_SystemReset();
			}
		}
		else if (state == STATE_DMA_WAIT_SIZE && dma_buffer[1] != 0x00) {
			if (dma_buffer[0] != 0xa6) {
				LL_USART_ReceiveData8(ESP_USART); // just for fun, i mean it might fix something?
				start_recv();
				return;
			}
			
			recv_full();
			last_comm = timekeeper.current_time;
		}
		else {
			process_command();
			start_recv();
			last_comm = timekeeper.current_time;
			sent_ping = false;
		}
	}
	else {
		LL_DMA_DisableStream(UART_DMA, UART_DMA_TX_Stream);

		while (!LL_USART_IsActiveFlag_TC(ESP_USART)) {
			;
		}

		is_sending = false;
	}
}

void srv::Servicer::cancel_recv() {
	LL_DMA_DisableStream(UART_DMA, UART_DMA_RX_Stream);
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
}
