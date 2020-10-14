#include "serial.h"
#include "esp_system.h"
#include "debug.h"
#include "wifitime.h"

#include <algorithm>
#include <ctime>
#include <esp_log.h>
#include <sys/time.h>

serial::SerialInterface serial::interface;

#define STASK_BIT_PACKET 1
#define STASK_BIT_REQUEST 2

static const char * TAG = "servicer";
static const uint32_t max_single_update_size = 1024;

void serial::SerialInterface::run() {
	// Initialize queue
	requests = xQueueCreate(4, sizeof(Request));
	xTaskNotifyStateClear(NULL);

	srv_task = xTaskGetCurrentTaskHandle();
	// Initialize the protocol
	init_hw();

	ESP_LOGI(TAG, "Initialized servicer HW");

	// Do a handshake
	while (true) {
		// Wait for a packet
		while (wait_for_event() == EventQueue) {;}
		// Check what packet we received
		auto cmd = rx_buf[2];
		continue_rx();
		if (cmd != slots::protocol::HANDSHAKE_INIT) {
			ESP_LOGW(TAG, "Invalid handshake init");
			continue;
		}

		// Send out a handshake_ok
		uint8_t buf_reply[3] = {
			0xa6,
			0x00,
			slots::protocol::HANDSHAKE_RESP
		};

		send_pkt(buf_reply);

		// Wait for another packet
		while (wait_for_event() == EventQueue) {;}
		// Check what packet we received
		cmd = rx_buf[2];
		continue_rx();
		if (cmd != slots::protocol::HANDSHAKE_OK) {
			ESP_LOGW(TAG, "Invalid handshake response");
			continue;
		}
		break;
	}

	ESP_LOGI(TAG, "Connected to STM32");

	// Start processing requests
	if (xQueueReceive(requests, &active_request, 0))
		start_request();

	while (true) {
		auto event = wait_for_event(active_request.type == Request::TypeEmpty ? pdMS_TO_TICKS(10000) : pdMS_TO_TICKS(1000));
		// this should be a switch...
		if (event == EventPacket || event == EventAll) {
			if (active_request.type == Request::TypeEmpty)
				process_packet();
			else {
				if (!packet_request())
					process_packet();
			}

			if (event == EventAll) goto check_request;
		}
		else if (event == EventQueue) {
check_request:
			if (active_request.type == Request::TypeEmpty && xQueueReceive(requests, &active_request, 0))
				start_request();
		}
		else {
			if (active_request.type != Request::TypeEmpty) {
				loop_request();
			}
			else {
				if (xQueueReceive(requests, &active_request, 0)) {
					ESP_LOGW(TAG, "Forgot about a request, starting it now");
					start_request();
				}
				else {
					// TODO: ping stuff
				}
			}
		}

		if (should_check_request) {
			should_check_request = false;
			goto check_request;
		}
	}
}

void serial::SerialInterface::reset() {
	Request r;
	r.type = Request::TypeReset;
	xQueueSend(requests, &r, portMAX_DELAY);
	xTaskNotify(srv_task, STASK_BIT_REQUEST, eSetBits);
}

void serial::SerialInterface::process_packet() {
	switch (rx_buf[2]) {
		case slots::protocol::PING:
			{
				ESP_LOGD(TAG, "got ping");
				continue_rx();

				uint8_t resp[3] = {
					0xa6,
					0x00,
					slots::protocol::PONG
				};

				send_pkt(resp);
			}
			break;
		case slots::protocol::QUERY_TIME:
			{
				ESP_LOGD(TAG, "got timereq");
				uint8_t resp[12] = {
					0xa6,
					9,
					slots::protocol::QUERY_TIME,
					0,
					0, 0, 0, 0, 0, 0, 0, 0
				};
				
				// Check if the time is set
				if (xEventGroupGetBits(wifi::events) & wifi::TimeSynced) {
					uint64_t millis = wifi::get_localtime();
					millis -= get_processing_delay();
					continue_rx();
					resp[3] = 0;
					memcpy(resp + 4, &millis, sizeof(millis));
				}
				else {
					continue_rx();
					resp[3] = (uint8_t)slots::protocol::TimeStatus::NotSet;
				}

				send_pkt(resp);
			}
			break;
		case slots::protocol::DATA_TEMP:
			{
				// When we receive a temperature message we have to do one of a few things:
				// If we've never seen this block:
				// 	- Create an empty placeholder
				// If we have this block:
				//  Set the temperature
				// 	If we're moving from cold -> hot,warm:
				// 	 If there are remote blocks:
				// 	  Mark all the blocks as flush
				//   Queue a block update: blocks are kept updated in terms of dirty but only flushed
				//                         if temperature is hot enough
				uint16_t slotid; memcpy(&slotid, rx_buf + 3, 2);
				uint8_t  reqtemp = rx_buf[5];
				continue_rx();

				ESP_LOGD(TAG, "Setting %03x to %d", slotid, reqtemp);

				// Check if this is a new block
				if (!arena.contains(slotid)) {
					// If it is, just put a placeholder
					if (!arena.add_block(slotid, bheap::Block::LocationCanonical, 0)) {
						ESP_LOGW(TAG, "Ran out of space trying to allocate placeholder for %03x", slotid);
						// Ignore and continue
						break;
					}
					ESP_LOGD(TAG, "Added empty placeholder");
					arena.set_temperature(slotid, reqtemp);
				}
				else {
					// Update the temperature
					arena.set_temperature(slotid, reqtemp);
					// Check if we need to flush "the rest" of the block.
					if (reqtemp & 0b10) {
						bool needs_update = false;

						if (std::any_of(arena.begin(slotid), arena.end(slotid), [](const auto &x){return x.location == bheap::Block::LocationRemote;}))
							needs_update = true;
						// If we need to send anything, send it
						if (needs_update) {
							ESP_LOGD(TAG, "Queueing block flushes since there were unflushed remote blocks");
							for (auto it = arena.begin(slotid); it != arena.end(slotid); ++it) {
								if (it->location == bheap::Block::LocationCanonical && !(it->flags & bheap::Block::FlagFlush)) {
									it->flags |= bheap::Block::FlagFlush;
								}
							}
						}
						update_blocks();
					}
				}

				// Send a response ack
				uint8_t response[6] = {
					0xa6,
					0x3,
					slots::protocol::ACK_DATA_TEMP,
					0,
					0,
					reqtemp
				};
				memcpy(response + 3, &slotid, 2);
				send_pkt(response);
			}
			break;
		case slots::protocol::DATA_MOVE:
			{
				// The STM is trying to move something to us. Due to restrictions on the STM, it only ever sends us packets with a max size of 64 bytes total, and so never
				// uses the framing mechanism (which is very useful)
				//
				// As a result this is fairly easy to implement.
				//
				// If we're currently evicting stuff though we null-ack this and delete the entire slot.
				// This _could_ cause problems but shouldn't if I haven't screwed up somewhere.

				uint16_t slotid; memcpy(&slotid, rx_buf + 3, 2);
				if (is_evicting) {
					// Send a null ack
					slotid &= ~0b1100'0000'0000'0000;
					continue_rx();

					uint8_t resp[5] = {
						0xa6,
						0x02,
						slots::protocol::DATA_DEL,
						0, 0
					};
					memcpy(resp + 3, &slotid, 2);
					send_pkt(&resp);

					ESP_LOGW(TAG, "Ingoring STM move attempt while evicting; deleting slot %03x", slotid);

					// Completely truncate the slot
					arena.truncate_contents(slotid, 0);
					// Ignore any ACKs (will be caught by the unknown whatever)
					break;
				}

				uint8_t result_code = 0;
				uint16_t offset; memcpy(&offset, rx_buf + 5, 2);
				uint16_t total_upd_len; memcpy(&total_upd_len, rx_buf + 9, 2);

				// Otherwise, we ensure that this is a supported packet
				if (!(slotid & ((1 << 14) | (1 << 15)))) {
					ESP_LOGE(TAG, "Ignoring STM move attempt with nonboring framing.");
					result_code = 3;
				}
				else {
					slotid &= ~0b1100'0000'0000'0000;
					// Otherwise process the request.
					if (!arena.set_location(slotid, offset, total_upd_len, bheap::Block::LocationCanonical)) {
						ESP_LOGE(TAG, "Failed to set locationcanonical for %03x", slotid);
						result_code = 1;
					}
					else {
						// Just set it
						if (!arena.update_contents(slotid, offset, total_upd_len, rx_buf + 11, false)) {
							ESP_LOGE(TAG, "Failed to update contents for move");
							result_code = 3;
						}
					}
				}

				// Send an ack packet
				uint8_t resp[10]  = {
					0xa6,
					7,
					slots::protocol::ACK_DATA_MOVE,
					0, 0,
					0, 0,
					0, 0,
					result_code
				};
				memcpy(resp + 3, rx_buf + 3, 2);
				continue_rx();
				memcpy(resp + 5, &offset, 2);
				memcpy(resp + 7, &total_upd_len, 2);
				send_pkt(&resp);

				ESP_LOGD(TAG, "Moved packet back to esp.");
			}
			break;
		default:
			ESP_LOGW(TAG, "Unknown packet type %02x", rx_buf[2]);
			continue_rx();
			break;
	}
}

void serial::SerialInterface::start_request() {
	switch (active_request.type) {
		case Request::TypeReset:
			{
				ESP_LOGE(TAG, "System is going down for reset.");
				uint8_t resp[3] = {
					0xa6,
					0x00,
					slots::protocol::RESET
				};
				send_pkt(resp);
				vTaskDelay(1000);
				esp_restart();
			}
			break;
		case Request::TypeDataResizeRequest:
			{
				ESP_LOGD(TAG, "Set size of %03x to %d", active_request.sparams.slotid, active_request.sparams.newsize);
				uint16_t current_size = arena.contents_size(active_request.sparams.slotid);
				if (current_size == active_request.sparams.newsize) {
					ESP_LOGD(TAG, "Already at that size, returning.");
					finish_request();
					return;
				}
				if (!arena.contains(active_request.sparams.slotid) || current_size < active_request.sparams.newsize) {
retry:
					ESP_LOGD(TAG, "Expanding to fit");
					if (!arena.add_block(active_request.sparams.slotid, bheap::Block::LocationCanonical, active_request.sparams.newsize - current_size)) {
						ESP_LOGW(TAG, "Running out of space while expanding slot, starting evict loop");
						evict_subloop(4 + active_request.sparams.newsize - current_size); // This will spin forever if it can't free
						goto retry; // this needs a timeout though just in case
					}
					finish_request();
				}
				else {
					// Truncate it
					arena.truncate_contents(active_request.sparams.slotid, active_request.sparams.newsize);
					if (active_request.sparams.newsize == 0) {
						// Use DATA_DEL
						ESP_LOGD(TAG, "Sending DATA_DEL");
						uint8_t resp[5] = {
							0xa6,
							0x02,
							slots::protocol::DATA_DEL,
							0, 0
						};
						memcpy(resp + 3, &active_request.sparams.slotid, 2);
						send_pkt(resp);
						return; // Wait for ack
					}
					else finish_request();
				}
			}
			break;
		case Request::TypeDataUpdateRequest:
			{
				ESP_LOGD(TAG, "Updating contents for %03x", active_request.uparams->slotid);
				// Update the contents in the arena. If that fails, the caller screwed up and we should just log and bail.
				if (!arena.update_contents(active_request.uparams->slotid, active_request.uparams->offset, active_request.uparams->length, active_request.uparams->data, true)) {
					ESP_LOGW(TAG, "Failed to update arena contents for %03x; bailing.", active_request.uparams->slotid);
					xTaskNotify(active_request.uparams->notify, 0xff, eSetValueWithOverwrite);
					finish_request();
					return;
				}
				// Notify requesting task
				xTaskNotify(active_request.uparams->notify, 0xee, eSetValueWithOverwrite);
				ESP_LOGD(TAG, "Finishing request");
				finish_request();
				// Otherwise trigger an update
				update_blocks();
				return;
			}
		default:
			ESP_LOGE(TAG, "Not implemented: ignoring request");
			finish_request();
			return;
	}
}

bool serial::SerialInterface::packet_request() {
	if (active_request.type != Request::TypeDataResizeRequest) {
		ESP_LOGE(TAG, "Not implemented: ignoring request packet");
		return false;
	}

	// Was the packet an ack data del?
	if (rx_buf[2] == slots::protocol::ACK_DATA_DEL) {
		// Is this a data delete for the right slot?
		if (memcmp(rx_buf + 3, &active_request.sparams.slotid, 2) == 0) {
			ESP_LOGD(TAG, "Finished DATA_DEL");
			finish_request();
		}
		else {
			ESP_LOGW(TAG, "Got unexpected DATA_DEL msg");
		}
		continue_rx();
		return true;
	}
	else return false;
}

void serial::SerialInterface::loop_request() {
	if (active_request.type != Request::TypeDataResizeRequest) {
		ESP_LOGE(TAG, "Not implemented: ignoring request loop");
		finish_request();
	}
	// TODO: some sort of repeater idk
	return;
}

void serial::SerialInterface::finish_request() {
	active_request.type = Request::TypeEmpty;
	should_check_request = true;
}

serial::SerialInterface::Event serial::SerialInterface::wait_for_event(TickType_t t) {
	uint32_t f;
rewait:
	if (xTaskNotifyWait(0, STASK_BIT_PACKET | STASK_BIT_REQUEST, &f, t) == pdFAIL) {
		return EventTimeout;
	}
	if ((f & (STASK_BIT_PACKET | STASK_BIT_REQUEST)) == (STASK_BIT_PACKET | STASK_BIT_REQUEST)) return EventAll;
	if (f & STASK_BIT_PACKET) return EventPacket;
	if (f & STASK_BIT_REQUEST) return EventQueue;
	goto rewait;
}

void serial::SerialInterface::on_pkt() {
	// Send a notification
	xTaskNotify(srv_task, STASK_BIT_PACKET, eSetBits);
}

// Offset is from start of block
uint8_t serial::SerialInterface::update_block_segment(slots::protocol::Command type_of_update, const bheap::Block& block, uint16_t offset_of_subupdate, uint16_t length, bool& request_occurred) {
	// Allocate space for an update
	uint8_t update_payload[258];
	memset(update_payload, 0, sizeof(update_payload));

	// Grab the offset of this block
	uint16_t offset = arena.block_offset(block) + offset_of_subupdate;

	ESP_LOGD(TAG, "Updating block %03x (@%04x / %dbytes / temp %d) with %d", block.slotid, offset, block.datasize, block.temperature, type_of_update);

	// Start sending update packets
	uint16_t current_offset = offset;
	uint16_t remaining = length;
	uint16_t slotsize = arena.contents_size(block.slotid);
	uint16_t upd_len = remaining; // this has to be set here b.c. bitfields
	while (true) {
		bool start  = current_offset == offset;
		bool end    = remaining <= 247;
		uint16_t sid_frame = (start ? (1 << 15):0) | (end ? (1 << 14):0) | block.slotid;

		update_payload[0] = 0xa6;
		update_payload[2] = type_of_update;
		memcpy(update_payload + 3, &sid_frame, 2);
		memcpy(update_payload + 5, &current_offset, 2);
		memcpy(update_payload + 7, &slotsize, 2);
		memcpy(update_payload + 9, &upd_len, 2);

		if (remaining > 247) {
			memcpy(update_payload + 11, (uint8_t *)block.data() + (current_offset - offset), 247);
			current_offset += 247;
			remaining -= 247;
			update_payload[1] = 255;
			send_pkt(update_payload);
		}
		else {
			memcpy(update_payload + 11, (uint8_t *)block.data() + (current_offset - offset), remaining);
			update_payload[1] = 8 + remaining;
			send_pkt(update_payload);
			break;
		}
	}

	uint8_t result = 0xff;
	// This is hoisted here because process_packet can invalidate references to block.
	uint16_t slotid = block.slotid;

	int retries;
	// Wait for a reply
	for (retries = 0; retries < 3;) {
		switch (wait_for_event(pdMS_TO_TICKS(2000))) {
			case EventAll:
				request_occurred = true;
			case EventPacket:
				{
					// Is this an update ack?
					if (rx_buf[2] != (type_of_update | 0x30)) {
						// No, process normally
						process_packet();
						continue;
					}
					// Is this the correct packet?
					if (!(memcmp(rx_buf + 3, &slotid, 2) == 0 && memcmp(rx_buf + 5, &offset, 2) == 0)) {
						// TODO: should this mark a thingy not-dirty?
						ESP_LOGW(TAG, "Unexpected ACK parameters in update, ignoring");
						continue_rx();
						++retries;
						continue;
					}
					// Otherwise, was it succesful?
					result = rx_buf[9];
					continue_rx();
					goto exit_retryloop1;
				}
				continue;
			case EventQueue:
				request_occurred = true;
				continue;
			case EventTimeout:
				++retries;
				continue;
		}
	}
exit_retryloop1:
	if (retries >= 3) {
		ESP_LOGW(TAG, "Timed out waiting for update ACK");
	}
	else ESP_LOGD(TAG, "Updated block OK");
	return result;
}

void serial::SerialInterface::update_blocks() {
tail_call_recurse:
	// Prevent recursion
	if (is_updating) {
		update_check_dirty = true;
		return;
	}

	update_check_dirty = false;

	is_updating = true;

	bool request_occurred = false;

	// First, we check if there are any dirty blocks with temperature that means we have to send them
	for (bheap::Block& block : arena) {
		if (!block) continue;
		if (block.location == bheap::Block::LocationCanonical && block.temperature & 0b10 && block.datasize && block.flags & bheap::Block::FlagDirty) {
			auto slotid = block.slotid;
			auto actual_offset = arena.block_offset(block);
			// Send this as a DATA_UPDATE packet.
			for (uint16_t subupdate_offset = 0; subupdate_offset < arena.get(slotid, actual_offset).datasize; subupdate_offset += max_single_update_size) {
				switch (
					update_block_segment(slots::protocol::DATA_UPDATE, arena.get(slotid, actual_offset), subupdate_offset, std::min<uint16_t>(max_single_update_size, arena.get(slotid, actual_offset).datasize - subupdate_offset), request_occurred)
				) {
					case 0:
						// success
						arena.get(slotid, actual_offset).flags &= ~bheap::Block::FlagDirty;
						break;
					default:
						// error; just give up and try again alter
						break;
				}
			}
		}

		// Try to flush out blocks
		//
		// Important note: this _isn't_ used in the evict loop because that function deals with failures differently
		if (block.location == bheap::Block::LocationCanonical && block.datasize && block.flags & bheap::Block::FlagFlush) {
			bool moved_ok = true; bool out_of_space = false;
			auto slotid = block.slotid;
			auto actual_offset = arena.block_offset(block);
			// Flush the entire block
			for (uint16_t subupdate_offset = 0; subupdate_offset < block.datasize; subupdate_offset += max_single_update_size) {
				switch (
					update_block_segment(slots::protocol::DATA_MOVE, arena.get(slotid, actual_offset), subupdate_offset, std::min<uint16_t>(max_single_update_size, arena.get(slotid, actual_offset).datasize - subupdate_offset), request_occurred)
				) {
					case 0:
						break;
					case 2:
					case 3:
					default:
						moved_ok = false;
						ESP_LOGW(TAG, "invalid state/timeout while trying to move block");
						break;
					case 1:
						out_of_space = true;
						ESP_LOGW(TAG, "out of space while trying to move block");
						break;
				}
			}
			if (moved_ok || out_of_space) {
				// Still clear the flag since trying again won't fix it rn.
				// At some point in the future the STM will probably try evicting stuff, so let it do that.
				arena.get(slotid, actual_offset).flags &= ~bheap::Block::FlagFlush;
			}
			if (moved_ok) {
				// success; the block is gone: update into a remote block.
				arena.set_location(slotid, actual_offset, arena.get(slotid, actual_offset).datasize, bheap::Block::LocationRemote);
			}
		}
	}

	// If we need to begin processing a request, start processing one
	if (request_occurred && active_request.type != Request::TypeEmpty && xQueueReceive(requests, &active_request, 0))
		start_request();

	is_updating = false;

	// If we need to continue processing, do a tail call
	if (update_check_dirty)
		goto tail_call_recurse;
}

void serial::SerialInterface::update_slot(uint16_t slotid, const void *ptr, size_t length) {
	allocate_slot_size(slotid, length);
	update_slot_partial(slotid, 0, ptr, length);
}

void serial::SerialInterface::allocate_slot_size(uint16_t slotid, size_t size) {
	Request pr;
	pr.type = Request::TypeDataResizeRequest;
	pr.sparams.slotid = slotid;
	pr.sparams.newsize = (uint16_t)size;
	xQueueSendToBack(requests, &pr, portMAX_DELAY);
	xTaskNotify(srv_task, STASK_BIT_REQUEST, eSetBits);
}

void serial::SerialInterface::update_slot_partial(uint16_t slotid, uint16_t offset, const void * ptr, size_t length) {
	Request pr;
	pr.type = Request::TypeDataUpdateRequest;
	UpdateParams up;
	up.data = ptr;
	up.offset = offset;
	up.slotid = slotid;
	up.length = length;
	up.notify = xTaskGetCurrentTaskHandle();
	pr.uparams = &up;
	xQueueSendToBack(requests, &pr, portMAX_DELAY);
	xTaskNotify(srv_task, STASK_BIT_REQUEST, eSetBits);
	xTaskNotifyWait(0, 0xffff'ffff, nullptr, portMAX_DELAY);
}

void serial::SerialInterface::evict_subloop(uint16_t amt) {
	ESP_LOGD(TAG, "evict subloop for %d bytes starting", amt);

	if (is_evicting) {
		ESP_LOGW(TAG, "evict recursion");
		return;
	}

	// Check if we're a) not updating blocks and b) have any flushable blocks
	if (!is_updating && std::any_of(arena.begin(), arena.end(), [](auto &b){return b && b.flags & bheap::Block::FlagFlush;})) {
		update_blocks();
	}

	// Check if we have enough space again after potentially flushing out blocks.
	if (arena.free_space(arena.FreeSpaceAllocatable) >= amt) {
		return;
	}
	
	// First we try a normal defrag
	arena.defrag();

	if (arena.free_space(arena.FreeSpaceAllocatable) >= amt) return;

	// This section is similar to the STM implementation; but probably won't get hit very much since ephemeral blocks usually aren't present
	//
	// We free from left to right; this could probably be more efficient going right to left but it'd use more memory more of the time.
	for (auto& block : arena) {
		if (block && block.location == bheap::Block::LocationEphemeral && block.datasize) {
			// Delete this block (shrink to size = 0 to create empty set location to remote and reset datasize)
			arena.set_location(block.slotid, arena.block_offset(block), block.datasize, bheap::Block::LocationRemote);
		}
	}
	arena.defrag();

	// Check if we have enough space
	if (arena.free_space(arena.FreeSpaceAllocatable) >= amt) {
		return;
	}

	// Still no?
	//
	// Start a loop of trying to send blocks to the STM

	is_evicting = true;
	bool request_occurred = false;
	int remaining_amount;

	auto choose_block = [this](auto&& cb){
		bheap::Block *chosen_one = nullptr;
		uint32_t chosen_size = 0 ;
		for (auto& block : this->arena) {
			if (cb(block)) {
				if (block.datasize > chosen_size || !chosen_size) {
					chosen_size = block.datasize;
					chosen_one = &block;
				}
			}
		}
		return chosen_one;
	};

	remaining_amount = amt - arena.free_space(arena.FreeSpaceAllocatable);

	do {
		 
		// Try to select a good block to remove.
		//
		// The best candidate to move is a cold large block. The larger blocks work better
		// because they should result in less negotiating.
		//
		// Additionally, any block <8 bytes in size is going to cause more problems from moving then it'll solve in general.

		// Find the largest block that:
		// 	- is at least 8 bytes large
		// 	- is warm
		// 	- is canonical

		bheap::Block *chosen_one = choose_block([](const bheap::Block& block){
			return block && block.location == bheap::Block::LocationCanonical && block.datasize >= 8 && block.temperature == bheap::Block::TemperatureWarm;
		});

		if (chosen_one) goto xfer_block;

		// Otherwise, try finding one that is cold
		chosen_one = choose_block([](const bheap::Block& block){
			return block && block.location == bheap::Block::LocationCanonical && block.datasize >= 8 && block.temperature == bheap::Block::TemperatureCold;
		});

		if (chosen_one) goto xfer_block;

		// Otherwise try a hot one
		chosen_one = choose_block([](const bheap::Block& block){
			return block && block.location == bheap::Block::LocationCanonical && block.datasize >= 4 && block.temperature == bheap::Block::TemperatureHot;
		});

		if (chosen_one) goto xfer_block;

		// If that fails, we're clearly screwed so just give up

		ESP_LOGE(TAG, "Completely screwed; no blocks to free. Exiting evict subloop");
		is_evicting = false;
		return;

xfer_block:
		uint32_t slotid = chosen_one->slotid;
		uint16_t offset = arena.block_offset(*chosen_one);
		uint16_t maxoff = offset + chosen_one->datasize;

		while (offset < maxoff) {
			const auto& block = arena.get(slotid, offset);
			auto sent_size = std::min<uint16_t>(max_single_update_size, block.datasize);

			if (remaining_amount <= 0) {
				ESP_LOGD(TAG, "Done early while evicting");
				break;
			}

			// Since is_evicting is set here, no changes are allowed to occur to the heap structure. [citation needed]
			switch (
				update_block_segment(slots::protocol::DATA_MOVE, block, 0, sent_size, request_occurred)
			) {
				default:
					ESP_LOGE(TAG, "Unknown status code, treating as OK");
				case 0:
					ESP_LOGD(TAG, "Moved subblock ok, freeing up space");
					if (!arena.set_location(slotid, offset, sent_size, bheap::Block::LocationRemote)) {
						ESP_LOGE(TAG, "Failed to update location.");
						goto break_sendloop;
					}
					// Increase for next offset
					offset += sent_size;
					remaining_amount = amt - arena.free_space(arena.FreeSpaceAllocatable);
					break;
				case 0xff:
					ESP_LOGW(TAG, "Timeout while trying to free up space, trying again.");
					break;
				case 2:
				case 3:
					ESP_LOGE(TAG, "Error in parameters while trying to send block. Trying a different block.");
					continue;
				case 1:
					ESP_LOGE(TAG, "Out of space from STM, abandoning");
					goto break_sendloop;
			}
		}
	} while (remaining_amount > 0);

	ESP_LOGD(TAG, "Succesfully freed up space.");
break_sendloop:
	is_evicting = false;

	// If we need to begin processing a request, start processing one
	if (request_occurred && active_request.type != Request::TypeEmpty && xQueueReceive(requests, &active_request, 0))
		start_request();

	return;
}
