#include "serial.h"
#include "esp_system.h"
#include "wifitime.h"

#include <algorithm>
#include <ctime>
#include <esp_log.h>
#include <sys/time.h>

serial::SerialInterface serial::interface;

static const char * TAG = "servicer";

static const char * TASK_TAGS[9] = {
	"servicer.task0",
	"servicer.task1",
	"servicer.task2",
	"servicer.task3",
	"servicer.task4",
	"servicer.task5",
	"servicer.task6",
	"servicer.task7",
	"servicer.task"
};

#define ST_LOGE( format, ... ) ESP_LOGE(TASK_TAGS[get_task_id()], format, ##__VA_ARGS__)
#define ST_LOGW( format, ... ) ESP_LOGW(TASK_TAGS[get_task_id()], format, ##__VA_ARGS__)
#define ST_LOGI( format, ... ) ESP_LOGI(TASK_TAGS[get_task_id()], format, ##__VA_ARGS__)
#define ST_LOGD( format, ... ) ESP_LOGD(TASK_TAGS[get_task_id()], format, ##__VA_ARGS__)
#define ST_LOGV( format, ... ) ESP_LOGV(TASK_TAGS[get_task_id()], format, ##__VA_ARGS__)

namespace serial::st {

	// BlockIn is a bit of a weird task; it doesn't do any of its initialization, instead it pretty 
	// much only handles receiving packets -- if there isn't enough space to hold an update we don't
	// bother starting this task in the first place.
	struct BlockInTask final : public serial::SerialSubtask {
		const static inline int TYPE = 4;

		BlockInTask(const uint8_t *current_pkt, uint8_t status_code=0) :
			current_pkt(current_pkt), status_code(status_code) {

			uint16_t sid_frame;
			memcpy(&sid_frame, current_pkt + 3, 2);

			slotid = sid_frame & 0xfff;
		}

		void start() override {
			// TODO: add a timeout for waiting for packets.

			if (current_pkt == nullptr) {
				ST_LOGW("current pkt is nullptr; exiting now -- may be stuck");
				finish();
				return;
			}

			uint16_t hdr[4]; memcpy(hdr, current_pkt + 3, sizeof(hdr));
			bool end   = hdr[0] & (1 << 14);

			if (!end && status_code) {
				// we've flagged an error, ignore this packet
				ST_LOGD("logged err, ignoring");
				return;
			}
			
			if (!status_code) {
				// Try to move this data into the heap
				if (!get_arena().update_contents(slotid, hdr[1], current_pkt[1] - sizeof(hdr), current_pkt + 3 + sizeof(hdr))) {
					// We failed to update contents; this task wouldn't be running if there was no space so the logical
					// error is invalid state
					status_code = 0x02;

					ST_LOGW("error in updating contents");
				}

				// Ensure this section of the heap is not dirty
				for (auto a = get_arena().begin(slotid); a != get_arena().end(slotid); ++a) {
					if (auto off = get_arena().block_offset(*a); off >= hdr[1] && off < hdr[1] + (current_pkt[1] - sizeof(hdr))) {
						a->flags = 0;
					}
				}
			}

			if (end) {
				uint8_t pkt[10] = {
					0xa6,
					0x7,
					slots::protocol::ACK_DATA_MOVE,
					0, 0,
					0, 0,
					0, 0,
					status_code
				};

				hdr[0] &= 0xfff;
				hdr[1] = hdr[1] + (current_pkt[1] - sizeof(hdr)) - hdr[3];
				hdr[2] = hdr[3];

				memcpy(pkt + 3, hdr, sizeof(uint16_t) * 3);

				ST_LOGD("finishing data move with code %d", status_code);

				send_packet(pkt);
				finish();
				return;
			}

			ST_LOGD("waiting for next packet");
		}

		int subtask_type() const override {return TYPE;}

		bool on_packet(uint8_t *pkt) override {
			if (pkt[2] != slots::protocol::DATA_MOVE) return false;
			uint16_t sid_frame; memcpy(&sid_frame, pkt + 3, 2);
			if ((sid_frame & 0xfff) != slotid) return false;

			current_pkt = pkt;
			start();
			current_pkt = nullptr;
			return true;
		}

	private:
		const uint8_t *current_pkt = nullptr;
		uint8_t status_code = 0;
	};

	// Sends out blocks corresponding to a single slotid
	struct BlockOutTask final : public serial::SerialSubtask {
		const static inline int TYPE = 2;

		BlockOutTask(uint16_t slotid) {
			this->slotid = slotid;
		}

		void start() override {
			// BlockOut tasks have the highest priority of all tasks, and so they don't delay for anything.
			//
			// Importantly, they also assume the summoner has done all the necessary checking to make sure
			// no two BlockOut tasks are operating on the same slotid.
			ST_LOGD("block out for slot %04x", slotid);

			send_chunk();
		};

		bool on_packet (uint8_t * packet) override {
			// Are we waiting for a packet right now?
			if (state < StWaitAck) return false;
			// Is this a DATA_MOVE / DATA_UPDATE_ACK?
			if (packet[2] != (state == StWaitAck ? slots::protocol::ACK_DATA_UPDATE : slots::protocol::ACK_DATA_MOVE)) return false;
			// Is this for the right slot?
			uint16_t hdr[3]; memcpy(hdr, packet + 3, sizeof(hdr));
			if ((hdr[0] & 0xFFF) != slotid) return false;
			
			ST_LOGD("got block out ack (@%04x, expect @%04x; len %04x, expect %04x)", hdr[1], offset, hdr[2], chunksize);

			uint8_t code = packet[9];
			switch (code) {
				case 0:
					// success
					handle_ack();
					// advance to next part of block
					offset += chunksize;
					// send next chunk
					send_chunk();
					break;
				case 1:
					// give up after a few attempts
					if (retries++ > 3) {
						ST_LOGW("gave up sending block due to no space");
						handle_ack();
						finish();
						return true;
					}
					// not enough space, wait a bit for transfers to come back to us and try again
					state = StDelayRetry;
					timeout = xTaskGetTickCount() + pdMS_TO_TICKS(250);
					break;
				default:
					ST_LOGW("block out errored, dropping update");
					// ack these but finish immediately
					handle_ack();
					finish();
					break;
			}
			return true;
		}

		void on_timeout() override {
			timeout = 0;
			switch (state) {
				default:
					return;
				case StWaitAck:
				case StWaitMoveAck:
					// Have we retried too many times?
					if (retries++ > 3) {
						// Cancel this update
						ST_LOGW("block out timed out, dropping update");
						// ack these but finish immediately
						handle_ack();
						finish();
					}
					// Otherwise, retry this transfer
					else {
				case StDelayRetry:
						send_chunk();
					}
					return;
			}
		}

		int subtask_type() const override {
			return TYPE;
		}

	private:
		static const inline uint32_t max_single_update_size = 1024;

		void handle_ack() {
			retries = 0;

			// Are we moving?
			if (state == StWaitMoveAck) {
				// Mark this chunk as remote
				get_arena().set_location(slotid, offset, chunksize, bheap::Block::LocationRemote);
			}
			// Clear all flags
			auto& block = get_arena().get(slotid, offset);
			block.flags = 0;
		}

		void send_chunk() {
again:
			// Try to find a block that needs updating
			auto& block = get_arena().get(slotid, offset);
			
			// All blocks have been updated, end this task
			if (!block) {
				finish();
				return;
			}

			ST_LOGD("try chose block %p (dsize %04x)", &block, block.datasize);

			// Does this block need updating?
			if (!(block.flags & bheap::Block::FlagFlush) && !((block.flags & bheap::Block::FlagDirty) && block.temperature >= bheap::Block::TemperatureWarm && block.location != bheap::Block::LocationRemote)) {
				// Try another block
				offset = get_arena().block_offset(block) + block.datasize;
				goto again;
			}

			// Start transferring at 0
			chunksize = block.datasize;

			// Is this block too long?
			if (block.datasize > max_single_update_size) {
				chunksize = max_single_update_size;
			}

			// We have the block, send the next part of this transfer
			uint8_t pkt[258]; bool end; uint16_t suboffset = 0;
			pkt[0] = 0xa6; pkt[2] = block.flags & bheap::Block::FlagFlush ? slots::protocol::DATA_MOVE : slots::protocol::DATA_UPDATE;
			do {
				bool start = suboffset == 0;
				end        = (suboffset + 247) > chunksize;
				// compute size
				if (end) {
					pkt[1] = chunksize - suboffset;
				}
				else {
					pkt[1] = 247;
				}
				{
					uint16_t hdr[4];
					// sid_frame
					hdr[0] = ((uint16_t)start << 15) | ((uint16_t)end << 14) | slotid;
					// offset
					hdr[1] = offset + suboffset;
					// slot_len
					hdr[2] = get_arena().contents_size(slotid);
					// total_len
					hdr[3] = chunksize;

					memcpy(pkt + 3, hdr, sizeof(hdr));
				}
				// Copy actual slot data
				memcpy(pkt + 3 + 8, suboffset + (uint8_t *)block.data(), pkt[1]);
				// Increment suboffset
				suboffset += pkt[1];
				// Add header size
				pkt[1] += 8;
				// Send this packet
				send_packet(pkt);
			} while (!end);

			// Setup wait
			state = block.flags & bheap::Block::FlagFlush ? StWaitMoveAck : StWaitAck;
			// Setup ack timeout
			timeout = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
		}

		uint16_t offset = 0, chunksize = 0, retries = 0;

		enum {
			StSendChunk0, // start full transfer
			StDelayRetry,
			StWaitAck,
			StWaitMoveAck,
		} state = StSendChunk0;
	};

	struct FreeSpaceTask final : public serial::SerialSubtask {
		const static inline int TYPE = 3;

		FreeSpaceTask(size_t required_space) :
			required_free_space(required_space) {
			ST_LOGD("free space task initializing with target = %zu", required_space);
		}

		void start() override {
			switch (state) {
				case StWaitForBlockToBeSent:
					// block was sent ok
					ST_LOGD("free sent %03x @%04x", chosen_slotid, chosen_offset);
				case StStart:
					ST_LOGD("free space defrag");
					// Perform a defragmentation of the heap
					get_arena().defrag();
					// Check if we have enough space
					if (auto x = get_arena().free_space(); x > required_free_space + buffer) {
						ST_LOGD("finished freeing space, exiting with %d bytes free", x);

						finish();
						return;
					}

					ST_LOGD("free space finding candidate");

					// Find a good candidate block
					{
						auto blk = find_candidate();
						if (!blk) {
							ST_LOGW("no candidate block found, exiting early");

							finish();
							return;
						}
						ST_LOGD("chose candidate %p", blk);
						// store block parameters in instance vars
						chosen_slotid = blk->slotid;
						chosen_offset = get_arena().block_offset(*blk);
					}

					// Check if something is using that block
				case StWaitForOldBlockOutToEnd:
					if (blocked_by_other_operation()) {
						ST_LOGD("old blockout still in progress");
						state = StWaitForOldBlockOutToEnd;
						// TODO: should there be a timeout here?
						return;
					}

					// Block is no longer in use, mark part of it for flushing.
					{
						auto& target = get_arena().get(chosen_slotid, chosen_offset);
						size_t length = target.datasize > max_single_move_size ? max_single_move_size : target.datasize;

						// Mark this chunk of the block as ephemeral 
						get_arena().set_location(chosen_slotid, chosen_offset, length, bheap::Block::LocationEphemeral);
						auto& newtarget = get_arena().get(chosen_slotid, chosen_offset);
						ST_LOGD("newtarget = %p; size = %04x", &newtarget, newtarget.datasize);
						newtarget.flags |= bheap::Block::FlagFlush;
					}

					mark_arena_dirty(true);
					state = StWaitForBlockToBeSent;
					return;
			}
		}

		void on_task_end(const SerialSubtask& task) override {
			if (state == StWaitForOldBlockOutToEnd) {
				start();
			}
			else if (state == StWaitForBlockToBeSent && task.subtask_type() == BlockOutTask::TYPE) {
				// Check if the block was actually sent out
				if (auto& blk = get_arena().get(chosen_slotid, chosen_offset); !blk || blk.location == bheap::Block::LocationRemote) {
					start();
				}
			}
		}

		int subtask_type() const override {
			return TYPE;
		}

	private:
		const static inline size_t buffer = 16;
		static const inline size_t max_single_move_size = 256;

		template<typename Pred>
		bheap::Block * find_candidate_matching(Pred&& candidate) {
			bheap::Block * best = nullptr;
			for (auto& blk : get_arena()) {
				if (candidate(blk)) {
					if (!best || best->datasize < blk.datasize) {
						best = &blk;
					}
				}
			}
			return best;
		}

		bheap::Block * find_candidate() {
			// First try and find a warm block (the idea being that it'll need to be on the stm anyways first)

			if (auto choice = find_candidate_matching([](const bheap::Block& block){
				return block && block.location == bheap::Block::LocationCanonical && block.datasize >= 8 && block.temperature == bheap::Block::TemperatureWarm;
			}); choice != nullptr) {
				return choice;
			}

			// Then try a cold block
			if (auto choice = find_candidate_matching([](const bheap::Block& block){
				return block && block.location == bheap::Block::LocationCanonical && block.datasize >= 8 && block.temperature == bheap::Block::TemperatureCold;
			}); choice != nullptr) {
				return choice;
			}

			// Then, lastly, try a hot block
			if (auto choice = find_candidate_matching([](const bheap::Block& block){
				return block && block.location == bheap::Block::LocationCanonical && block.datasize >= 8 && block.temperature == bheap::Block::TemperatureHot;
			}); choice != nullptr) {
				return choice;
			}

			// No good blocks to free.
			return nullptr;
		}

		bool blocked_by_other_operation() {
			return other_tasks_match([&](const SerialSubtask& b){
				return b.slotid == chosen_slotid && (
					b.subtask_type() == BlockOutTask::TYPE
				);
			});
		}

		size_t required_free_space;
		
		// chosen block
		uint16_t chosen_slotid;
		uint16_t chosen_offset;

		enum {
			StStart,
			StWaitForOldBlockOutToEnd,
			StWaitForBlockToBeSent,
		} state = StStart;
	};

	struct UpdateSizeTask final : public serial::SerialSubtask {
		const static inline int TYPE = 1;

		UpdateSizeTask(uint16_t slotid, uint16_t newsize) : new_size(newsize) {
			this->slotid = slotid;
		}

		void start() override {
			uint16_t current_size = get_arena().contains(slotid) ? get_arena().contents_size(slotid) : 0;
			uint8_t pkt[5] = {
				0xa6, 0x02, slots::protocol::DATA_DEL,
				0, 0
			};

			switch (state) {
				case StStart:
					if (blocked()) {
						state = StWaitUnblocked;
						return;
				case StWaitUnblocked:
						;
					}

					// Do we even need to do anything? (this happens after the blocked check
					// so multiple size updates don't cause big problems)
					if (current_size == new_size) {
						// no, we don't
						finish();
						return;
					}

					ST_LOGD("updating %03x to size %04x from %04x", slotid, new_size, current_size);

					// Are we expanding?
					if (!get_arena().contains(slotid) || current_size < new_size) {
						ST_LOGD("adding new block");
again:
						// Check if this should be a remote block (temp >= warm and any block is remote)
						// Try to allocate the space
						if (!get_arena().add_block(slotid, 
							std::any_of(get_arena().begin(slotid), get_arena().end(slotid), [](const auto &b){
								return b.temperature >= bheap::Block::TemperatureWarm && b.location == bheap::Block::LocationRemote;
							}) ? bheap::Block::LocationRemote : bheap::Block::LocationCanonical, new_size - current_size)
						) {
							// Out of space
							ST_LOGD("out of space updating contents, will cleanup.");
				case StWaitingForEmptySlotToFreeSpace:
							state = StWaitingForFreedSpace;
							if (try_start_task<FreeSpaceTask>(new_size - current_size) == -1) {
								ST_LOGD("delaying for empty slot");
								state = StWaitingForEmptySlotToFreeSpace;
								return;
							}
							return;
				case StWaitingForFreedSpace:
							ST_LOGD("freed space");
							goto again;
						}
						else {
							// Succesfully allocated, finish task
							finish();
							return;
						}
					}
					else {
						// Truncate
						get_arena().truncate_contents(slotid, new_size);
						// Did we completely erase it?
						if (new_size == 0) {
							// Send a delete message
							memcpy(pkt + 3, &slotid, 2);
							send_packet(pkt);
							state = StWaitForDelAck;
							timeout = xTaskGetTickCount() + 250;
							return;
				case StWaitForDelAck:
							ST_LOGD("got del ack ok");
						}
					}
					finish();
					return;
			}
		};

		bool on_packet(uint8_t *packet) override {
			if (state != StWaitForDelAck) return false;
			if (packet[2] != slots::protocol::ACK_DATA_DEL) return false;
			if (memcmp(packet + 3, &slotid, 2)) return false;

			// Got packet ok
			timeout = 0;
			start();
			return true;
		}

		void on_timeout() override {
			timeout = 0;
			if (state != StWaitForDelAck) return;
			ST_LOGW("del ack timed out");
			finish();
			return;
		}

		void on_task_end(const SerialSubtask& task) override {
			if (state == StWaitUnblocked) {
				if (!blocked()) start();
			}
			else if (state == StWaitingForFreedSpace) {
				if (task.subtask_type() == FreeSpaceTask::TYPE) {
					start();
				}
			}
			else if (state == StWaitingForEmptySlotToFreeSpace) 
				start();
		}

		int subtask_type() const override {
			return TYPE;
		}

		bool should_block_updates_for(uint16_t slotid) override {
			if (state == StWaitUnblocked || state == StWaitingForFreedSpace || state == StWaitingForEmptySlotToFreeSpace) return false;
			return slotid == this->slotid;
		}

	private:
		bool blocked() {
			// Update size won't run if any of the following tasks are active:
			//   - block in/out with same slotid
			//   - data flush

			return other_tasks_match([&](const SerialSubtask& s){
				return s.subtask_type() == FreeSpaceTask::TYPE ||
					((s.subtask_type() == BlockInTask::TYPE || s.subtask_type() == BlockOutTask::TYPE || s.subtask_type() == UpdateSizeTask::TYPE) && s.slotid == this->slotid);
			});
		}

		uint16_t new_size;

		enum {
			StStart,
			StWaitUnblocked,
			StWaitForDelAck,
			StWaitingForEmptySlotToFreeSpace,
			StWaitingForFreedSpace
		} state = StStart;
	};

	// Updates contents of a slot with data
	struct DataUpdateTask final : public serial::SerialSubtask {
		const static inline int TYPE = 5;

		DataUpdateTask(uint16_t slotid, uint16_t offset, const void * data, uint16_t length) :
			offset(offset), data(data), length(length) {

			this->slotid = slotid;
		}

		void start() override {
			switch (state) {
				case StStart:
					// Give up after 1.5 seconds
					if (this->timeout == 0) this->timeout = xTaskGetTickCount() + pdMS_TO_TICKS(1500);
				recheck_blocked:
					if (blocked()) {
						ST_LOGD("data update blocked");
						state = StWaitUnblocked;
						return;
				case StWaitUnblocked:
						ST_LOGD("data update unblocked");
					}

					ST_LOGD("updating slot %03x by length %04x", slotid, length);

				again:
					// Try to update
					if (!get_arena().update_contents(slotid, offset, length, data, true)) {
						// Check if we've correctly set the size
						if (offset + length > get_arena().contents_size(slotid)) {
							ST_LOGW("invalid slotid size during update, scheduling update");
				case StWaitingForEmptySlotToUpdateSize:
							state = StStart;
							if (try_start_task<UpdateSizeTask>(slotid, offset + length) == -1) {
								ST_LOGD("delaying for empty slot (size)");
								state = StWaitingForEmptySlotToUpdateSize;
								return;
							}
							goto recheck_blocked;
						}
						// Out of space
						ST_LOGD("out of space updating contents, will cleanup.");
				case StWaitingForEmptySlotToFreeSpace:
						state = StWaitingForFreedSpace;
						if (try_start_task<FreeSpaceTask>(length) == -1) {
							ST_LOGD("delaying for empty slot");
							state = StWaitingForEmptySlotToFreeSpace;
							return;
						}
						return;
				case StWaitingForFreedSpace:
						ST_LOGD("freed space");
						goto again;
					}

					// Mark the arena as having been modified
					mark_arena_dirty();

					ST_LOGD("finished");
					finish();
					return;
			}
		}

		void on_task_end(const SerialSubtask& task) override {
			if (state == StWaitUnblocked) {
				if (!blocked()) start();
			}
			else if (state == StWaitingForFreedSpace) {
				if (task.subtask_type() == FreeSpaceTask::TYPE) {
					start();
				}
			}
			else if (state == StWaitingForEmptySlotToFreeSpace || state == StWaitingForEmptySlotToUpdateSize) 
				start();
		}

		int subtask_type() const override {
			return TYPE;
		}

		bool should_block_updates_for(uint16_t slotid) override {
			if (state == StWaitUnblocked || state == StWaitingForFreedSpace || state == StWaitingForEmptySlotToFreeSpace) return false;
			return slotid == this->slotid;
		}

		// Handle giving up on timeout of ~1.5s
		void on_timeout() override {
			timeout = 0;
			ST_LOGW("timed out trying to update slot; giving up");
			finish();
		}

	private:
		bool blocked() {
			// Update data won't run if any of the following tasks are active:
			//   - block in/out with same slotid
			//   - data flush
			//   - update size still pending

			return other_tasks_match([&](const SerialSubtask& s){
				return s.subtask_type() == FreeSpaceTask::TYPE ||
					((s.subtask_type() == BlockInTask::TYPE || s.subtask_type() == BlockOutTask::TYPE || s.subtask_type() == UpdateSizeTask::TYPE || s.subtask_type() == DataUpdateTask::TYPE) && s.slotid == this->slotid);
			});
		}
		
		uint16_t offset;
		const void * data;
		uint16_t length;

		enum {
			StStart,
			StWaitUnblocked,
			StWaitingForEmptySlotToFreeSpace,
			StWaitingForFreedSpace,
			StWaitingForEmptySlotToUpdateSize
		} state = StStart;
	};

	struct SyncTask final : public SerialSubtask {
		static const inline int TYPE = 6;

		SyncTask(uint16_t *slotids, uint16_t amt, TaskHandle_t target) :
			to_notify(target), slotids(slotids), amt(amt) {
		}

		void on_task_end(const SerialSubtask&) override {
			if (state == StWaiting) {
				if (!waiting()) start();
			}
		}
		
		void start() override {
			switch (state) {
				case StStart:
					if (waiting()) {
						state = StWaiting;
						return;
				case StWaiting:
						ST_LOGD("wait finished, notifying");
					}

					xTaskNotify(to_notify, 0xaaaa5555, eSetValueWithOverwrite);
					finish();
					return;
			}
		}

		int subtask_type() const override {
			return TYPE;
		}
	private:
		TaskHandle_t to_notify;
		uint16_t *slotids;
		uint16_t amt;

		bool waiting() {
			return other_tasks_match([&](const SerialSubtask& s){
				return ((s.subtask_type() == DataUpdateTask::TYPE || s.subtask_type() == UpdateSizeTask::TYPE) && 
				std::find(slotids, slotids + amt, s.slotid) != slotids + amt);
			});
		}

		enum {
			StStart,
			StWaiting
		} state = StStart;
	};
};

int serial::SerialSubtask::get_task_id() const {
	for (int i = 0; i < 8; ++i) {
		if (interface.subtasks[i] == this) return i;
	}
	return 8;
}

void serial::SerialSubtask::finish() {
	ST_LOGD("finishing task");
	interface.finish_subtask(get_task_id());
}

template<typename Task, typename ...Args>
int serial::SerialInterface::try_start_task(Args&& ...args) {
	// Try to find an empty slot ID
	for (int i = 0; i < 8; ++i) {
		if (subtasks[i]) continue;

		// Print a log message for this task specifically
		ESP_LOGD(TASK_TAGS[i], "starting task with type %d", Task::TYPE);

		// Initialize task
		subtasks[i] = new Task(std::forward<Args>(args)...);
		
		// Start task
		subtasks[i]->start();

		return i;
	}

	return -1;
}

void serial::SerialInterface::mark_arena_dirty(bool ignore_defer) {
	// This function is perhaps a bit of a misnomer (at least in terms of what it actually does) -- _functionally_ it marks
	// the arena dirty and queues up block outs, but in reality it just does the queueing and/or tells the finish_subtask function
	// that it needs to call this again since it ran out of slots (or didn't want to make more than 3 block out tasks at once)
	
	const static int max_out_tasks = 3;

	arena_dirty = false;

	// If defer_arena_updates is set, though, we treat it as _if_ it just marked the arena dirty
	if (defer_arena_updates && !ignore_defer) {
		arena_dirty = true;
		return;
	}

	// We need to calculate some things in order to check whether blocks have enough space to be moved.
	//
	// For the purposes of this calculation, we treat the STM's heap as an ideal linear layout in two segments:
	//
	// |    hot    |  warm/DATA_MOVED  | free |
	//
	// By definition, hot blocks should be prioritized at all costs, so they are sent always and assumed to be always present.
	//
	// As a result, the "budget" for warm blocks / moved data is STM_HEAP_SIZE - sum(blk.size for blk in blocks if blk.temp == Hot).
	//
	// We have to prioritize moved blocks which won't fit on us over warm blocks, so we first have to compute how much space all remote blocks
	// take up. If there's corresponding free space on us, we can swap it (on the stm) with a warm block.
	
	ssize_t total_hot = 0;
	ssize_t moved_space = 0;

	for (auto& blk : arena) {
		if (blk.temperature == bheap::Block::TemperatureHot)
			total_hot += blk.datasize + 4; 
		else if ((blk.location == bheap::Block::LocationRemote || blk.flags & bheap::Block::FlagFlush) && blk.temperature <= bheap::Block::TemperatureWarm) 
			moved_space += blk.datasize + 4;
	}

	ssize_t warm_budget = STM_HEAP_SIZE - total_hot - moved_space - 4;
	if (warm_budget < 0) warm_budget = 0;

	ssize_t free_space_for_moved_blocks = arena.free_space() - 32;

	// Go through all blocks and schedule their slots
	for (auto& blk : arena) {
		if (!blk) continue;

		// only considered for warm blocks
		bool space_available = false;

		// Update the budgets so we can choose how to update this and future blocks
		if (blk.temperature == bheap::Block::TemperatureWarm) {
			// Is there space left in the budget?
			if (warm_budget >= blk.datasize) {
				space_available = true;
				// use budget
				warm_budget -= blk.datasize;
			}
			else {
				// Can we make more space by moving parts of the cold region here?
				if (moved_space > blk.datasize && free_space_for_moved_blocks > blk.datasize) {
					moved_space -= blk.datasize;
					free_space_for_moved_blocks -= blk.datasize;
					space_available = true;
				}
			}
		}

		// Check if this block needs updating
		if ((blk.flags & bheap::Block::FlagFlush) || (blk.temperature >= bheap::Block::TemperatureWarm && blk.location != bheap::Block::LocationRemote && (blk.flags & bheap::Block::FlagDirty))) {
			// Are we already updating this block?
			if (std::any_of(subtasks, subtasks + 8, [&](serial::SerialSubtask *task){
				return task && task->subtask_type() == st::BlockOutTask::TYPE && task->slotid == blk.slotid;
			})) {
				ESP_LOGD(TAG, "skipping %04x because it's still being sent", blk.slotid);
				// Ignore it, then
				continue;
			}

			// Are we processing this block somehwere else? If we are we should wait for it to finish being processed.
			if (std::any_of(subtasks, subtasks + 8, [&](serial::SerialSubtask *task){
				return task && task->should_block_updates_for(blk.slotid);
			})) {
				// Mark the arena as dirty so we try again later but don't actually stop processing
				ESP_LOGD(TAG, "delaying %04x because it's being blocked", blk.slotid);
				arena_dirty = true;
				continue;
			}

			// Do we have too many out tasks running?
			if (std::count_if(subtasks, subtasks + 8, [&](serial::SerialSubtask *task){
				return task && task->subtask_type() == st::BlockOutTask::TYPE;
			}) >= max_out_tasks) {
				// We're out of slots, so mark arena dirty and return now
				ESP_LOGD(TAG, "delaying %04x because we don't have enough slots", blk.slotid);
				arena_dirty = true;
				return;
			}

			// Does this warm block fit?
			if (blk.temperature == bheap::Block::TemperatureWarm && !space_available) {
				ESP_LOGW(TAG, "not sending warm block %04x since there's no space for it", blk.slotid);
				continue;
			}

			// Try to start a new task
			if (try_start_task<st::BlockOutTask>((uint16_t)blk.slotid) == -1) {
				// We've run of tasks in total, so again mark dirty and stop now
				ESP_LOGD(TAG, "delaying %04x because we couldn't start the task", blk.slotid);
				arena_dirty = true;
				return;
			}
		}
	}
}

void serial::SerialInterface::finish_subtask(int i) {
	if (!subtasks[i]) {
		ESP_LOGW(TAG, "finish subtask called with invalid index %d", i);
	}

	// Clear this task from the list
	SerialSubtask *task = std::exchange(subtasks[i], nullptr);
	
	// Inform all tasks that this task just ended
	for (const auto& x : subtasks) {
		if (!x) continue;
		x->on_task_end(*task);
	}

	// Clear out the task
	delete task;

	// If we are still trying to update the heap, re-run mark_arena_dirty
	if (arena_dirty)
		mark_arena_dirty(); // sets back to false if finished

	{
		DeferGuard g(this); // defer updates until all requests are handled
		// If there are any empty slots, try to handle stuff in the overflow queue
		while (std::any_of(subtasks, subtasks + i, [](auto x){return !x;})) {
			SerialEvent evt;
			if (xQueuePeek(request_overflow, &evt, 0)) {
				// Handle the request
				if (!handle_request(evt)) {
					ESP_LOGW(TAG, "despite there being an empty spot in the task list, request still failed -- keeping it in the queue");
					return;
				}
				else {
					xQueueReceive(request_overflow, &evt, 0);
				}
			}
			else {
				// Empty queue, stop processing
				return;
			}
		}
	}
}

bool serial::SerialInterface::handle_request(SerialEvent &evt) {
	switch (evt.event_subtype) {
		default:
			ESP_LOGE(TAG, "unknown request subtype %d", evt.event_subtype);
			return true;

		case SerialEvent::EventSubtypeSystemReset:
			{
				// End thine suffering a doest thou commit one holy reset  (TODO: handle advanced resets)
				uint8_t pkt[3] = {
					0xa6,
					0x00,
					slots::protocol::RESET
				};

				send_pkt(pkt);
				ESP_LOGE(TAG, "The system is going down for reset.");
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_restart();
			}
			return true;

		case SerialEvent::EventSubtypeDataUpdate:
			{
				// Queue up a data update task
				if (try_start_task<st::DataUpdateTask>(evt.d.data_update.slotid, evt.d.data_update.offset, evt.d.data_update.data, evt.d.data_update.length) == -1) {
					return false;
				}

				return true;
			}

		case SerialEvent::EventSubtypeSizeUpdate:
			{
				// Check if the slot size is already that, and if it is just skip this, but only if this is the only update size
				// task that'd be running for that slot.
				if (
					std::none_of(subtasks, subtasks + 8, [&](auto &i){return i && i->subtask_type() == st::UpdateSizeTask::TYPE && i->slotid == evt.d.size_update.slotid;}) &&
					arena.contents_size(evt.d.size_update.slotid) == evt.d.size_update.newsize
				) {
					// skip it
					ESP_LOGD(TAG, "skipping size update");
					return true;
				}
				// Queue up a size update task
				if (try_start_task<st::UpdateSizeTask>(evt.d.size_update.slotid, evt.d.size_update.newsize) == -1) {
					return false;
				}

				return true;
			}

		case SerialEvent::EventSubtypeSync:
			{
				// Queue up a syncing task
				if (try_start_task<st::SyncTask>(evt.d.sync.slotids, evt.d.sync.amt, evt.d.sync.tonotify) == -1) {
					return false;
				}

				return true;
			}
	}
}

void serial::SerialInterface::process_packet_directly() {
	switch (rx_buf[2]) {
		using namespace slots::protocol;

		case PING:
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

			return;

		case QUERY_TIME:
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

			return;
		case DATA_TEMP:
			// Data temp currently runs outside a task since it doesn't (or shouldn't, at any rate) mess up 
			// ongoing task state.
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
						// Ignore and continue (since we need the placeholder and can't just flush stuff away for it.
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
					}
					mark_arena_dirty();
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

			return;

		case DATA_FORGOT:
			// This also happens outside of the normal channels since it just messes with flags
			{
				// The STM says it deleted its copy of some of our data, so we mark the relevant section as dirty to compensate.
				uint16_t slotid, offset, length;
				memcpy(&slotid, rx_buf + 3, 2);
				memcpy(&offset, rx_buf + 5, 2);
				memcpy(&length, rx_buf + 7, 2);
				continue_rx();

				ESP_LOGD(TAG, "Marking region @%04x of length %d in slot %03x as dirty due to DATA_FORGOT", offset, length, slotid);

				bool should_flush = std::any_of(arena.begin(slotid), arena.end(slotid), [](const auto& b){return b.location == bheap::Block::LocationRemote;});

				for (auto *b = &arena.get(slotid, offset); b && arena.block_offset(*b) < offset + length; b = b->next()) {
					// Is this a canonical block? If so, we mark it dirty as long as it's not queued to be flushed
					if (b->location == bheap::Block::LocationCanonical && !(b->flags & bheap::Block::FlagFlush) && !should_flush) {
						b->flags |= bheap::Block::FlagDirty;
					}
					else if (b->location == bheap::Block::LocationRemote) {
						ESP_LOGW(TAG, "Block %p has not yet been moved back to the esp, so we're ignoring the request to clear it", b);
					}
					else if (should_flush) {
						// There is data here but the stm wants it flushed.
						ESP_LOGD(TAG, "Block should be flushed, flushing it");
						b->flags |= bheap::Block::FlagFlush;
					}
					else {
						ESP_LOGW(TAG, "Skipping %p", b);
					}
				}

				mark_arena_dirty();
			}
			
			return;
		case DATA_MOVE:
			// This is started here as a task (and if that task was ignored b.c. lack of space gets ignored here too)
			{
				
				uint16_t hdr[4]; memcpy(hdr, rx_buf + 3, sizeof(hdr));
				bool start = hdr[0] & (1 << 15);
				hdr[0] &= 0xfff;

				if (!start) {
					ESP_LOGW(TAG, "ignoring non-start data move");
					continue_rx();
					return;
				}

				uint8_t status = 0;
				
				// Check if there any ongoing data update tasks concerning this slot
				if (std::any_of(subtasks, subtasks + 8, [&](auto x){
					return x && (
						x->subtask_type() == st::BlockOutTask::TYPE ||
						x->subtask_type() == st::DataUpdateTask::TYPE ||
						x->subtask_type() == st::UpdateSizeTask::TYPE
					) && x->slotid == hdr[0];
				})) {
					status = 2;
				}
				else if (std::any_of(subtasks, subtasks + 8, [&](auto x){
					return x && ((
						x->subtask_type() == st::BlockInTask::TYPE && x->slotid == hdr[0]
					) || x->subtask_type() == st::FreeSpaceTask::TYPE);
				})) {
					status = 3;
				}
				else {
					// Try to allocate space
					if (!arena.set_location(hdr[0], hdr[1], hdr[3], bheap::Block::LocationCanonical)) {
						status = 1;
					}
				}

				// Start the task
				if (try_start_task<st::BlockInTask>(rx_buf, status) == -1) {
					// Ignore the packet
					ESP_LOGW(TAG, "out of space trying to get block, ignoring it");
				}

				continue_rx();
			}
			
			return;

		case slots::protocol::HANDSHAKE_INIT:
			{
				ESP_LOGW(TAG, "Got handshake init in main loop; resetting");
				esp_restart();
			}
			break;
		default:
			ESP_LOGW(TAG, "Unknown packet type %02x", rx_buf[2]);
			continue_rx();
			break;
	}
}

void serial::SerialInterface::on_pkt() {
	// Place an entry into the queue
	SerialEvent r;
	r.event_type = SerialEvent::EventTypePacket;
	xQueueSend(events, &r, portMAX_DELAY);
}

const static inline int max_single_processing_events = 6;

void serial::SerialInterface::run() {
	// Initialize queue
	events = xQueueCreate(4, sizeof(SerialEvent));
	xTaskNotifyStateClear(NULL);
	request_overflow = xQueueCreate(8, sizeof(SerialEvent));

	srv_task = xTaskGetCurrentTaskHandle();
	// Initialize the protocol
	init_hw();

	ESP_LOGI(TAG, "Initialized servicer HW");

	{
		bool state = false;
		// Do a handshake
		while (true) {
			SerialEvent evt;
			xQueueReceive(events, &evt, portMAX_DELAY);

			if (evt.event_type != SerialEvent::EventTypePacket) {
				ESP_LOGD(TAG, "early request, placing into overflow pile");
				if (!xQueueSend(request_overflow, &evt, 0)) {
					ESP_LOGW(TAG, "too many early requests, dropping this one");
				}
				continue;
			}

			// Is this the correct packet?
			auto cmd = rx_buf[2];
			continue_rx();

			if (!state) {
				if (cmd != slots::protocol::HANDSHAKE_INIT) {
					ESP_LOGW(TAG, "Invalid handshake init");
					continue;
				}
				state = true;

				// Send out a handshake_ok
				uint8_t buf_reply[3] = {
					0xa6,
					0x00,
					slots::protocol::HANDSHAKE_RESP
				};

				send_pkt(buf_reply);
				continue;
			}
			else {
				if (cmd == slots::protocol::HANDSHAKE_INIT) continue;
				if (cmd != slots::protocol::HANDSHAKE_OK) {
					state = false;
					ESP_LOGW(TAG, "Invalid handshake response");
					continue;
				}

				break;
			}
		}
	}

	ESP_LOGI(TAG, "Connected to STM32");

	// Process old requests
	while (true) {
		SerialEvent evt;
		if (!xQueuePeek(request_overflow, &evt, 0)) {
			break; // finished
		}

		// Try to handle it
		if (!handle_request(evt)) {
			ESP_LOGD(TAG, "ran out of space processing early request, delaying them for later");
			break;
		}
		else {
			// receieve it fully
			xQueueReceive(request_overflow, &evt, 0);
		}
	}

	// Main servicer loop
	while (true) {
		// Perform bookkeeping tasks
		update_task_timeouts();
		send_pings();

		DeferGuard g(this);
		SerialEvent evt;
		int processed = 0;
		do {
			if (!xQueueReceive(events, &evt, pdMS_TO_TICKS(100))) { // min timeout delay
				break;
			}

			++processed;

			// Did we get a packet?
			if (evt.event_type == SerialEvent::EventTypePacket) {
				// Try giving it to all subtasks before processing it ourselves.
				for (auto& subtask : subtasks) {
					if (!subtask) continue;
					if (subtask->on_packet(rx_buf)) {
						// handled packet, continue
						continue_rx();
						goto processed_by_subtask;
					}
				}

				// Packet needs to be handled separately.
				process_packet_directly();
	processed_by_subtask:
				;
			}
			else {
				// Is the overflow queue currently in use?
				if (uxQueueMessagesWaiting(request_overflow)) {
					// Send directly to overflow queue
					ESP_LOGD(TAG, "already overflowing, sending next requests there too");
					goto send_to_overflow;
				}
				// Handle the request
				if (!handle_request(evt)) {
					// Place it in the overflow queue
					ESP_LOGD(TAG, "placing request in overflow queue");
send_to_overflow:
					if (!xQueueSend(request_overflow, &evt, 0)) {
						ESP_LOGW(TAG, "out of space in overflow queue, dumping request");
					}
					// Break to perform bookkeeping tasks so we have a bit of time to potentially clear the obstruction
					break;
				}
			}

			// If we've processed more than max_single_processing events in one go, break to let the bookkeeping tasks run
			if (processed > max_single_processing_events) {
				break;
			}
		} while (uxQueueMessagesWaiting(events));
	}
}

void serial::SerialInterface::update_task_timeouts() {
	for (auto subtask : subtasks) {
		if (!subtask) continue;
		if (subtask->current_timeout() == 0 || subtask->current_timeout() > xTaskGetTickCount()) {
			continue;
		}
		subtask->on_timeout();
	}
}

void serial::SerialInterface::send_pings() {
	// TODO
}

void serial::SerialInterface::reset() {
	SerialEvent r;
	r.event_type = SerialEvent::EventTypeRequest;
	r.event_subtype = SerialEvent::EventSubtypeSystemReset;
	xQueueSend(events, &r, portMAX_DELAY);
}

void serial::SerialInterface::update_slot(uint16_t slotid, const void *ptr, size_t length, bool should_sync) {
	// TODO: syncing options
	allocate_slot_size(slotid, length);
	update_slot_partial(slotid, 0, ptr, length, should_sync);
}

void serial::SerialInterface::allocate_slot_size(uint16_t slotid, size_t size) {
	SerialEvent pr;
	pr.event_type = SerialEvent::EventTypeRequest;
	pr.event_subtype = SerialEvent::EventSubtypeSizeUpdate;
	pr.d.size_update.slotid = slotid;
	pr.d.size_update.newsize = (uint16_t)size;
	xQueueSendToBack(events, &pr, portMAX_DELAY);
}

void serial::SerialInterface::update_slot_partial(uint16_t slotid, uint16_t offset, const void * ptr, size_t length, bool should_sync) {
	{
		SerialEvent pr;
		pr.event_type = SerialEvent::EventTypeRequest;
		pr.event_subtype = SerialEvent::EventSubtypeDataUpdate;
		pr.d.data_update.slotid = slotid;
		pr.d.data_update.length = (uint16_t)length;
		pr.d.data_update.offset = offset;
		pr.d.data_update.data = ptr;
		xQueueSendToBack(events, &pr, portMAX_DELAY);
	}

	if (should_sync) {
		sync_slots(slotid);
	}
}

void serial::SerialInterface::sync_slots_array(uint16_t *sid_list, uint16_t sid_size) {
	SerialEvent pr;
	pr.event_type = SerialEvent::EventTypeRequest;
	pr.event_subtype = SerialEvent::EventSubtypeSync;
	pr.d.sync.slotids = sid_list;
	pr.d.sync.amt = sid_size;
	pr.d.sync.tonotify = xTaskGetCurrentTaskHandle();
	xQueueSendToBack(events, &pr, portMAX_DELAY);
	xTaskNotifyWait(0, 0xffff'ffff, nullptr, portMAX_DELAY);
}
