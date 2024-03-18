#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>

namespace crashlogs {
	// Stores crash logs or a normal type. The normal type is not destructed when the crashlogs run, so this should only be used
	// for some infinite-lifetime global.
	//
	// Objects of this type _must_ be declared _NOINIT for this to work across reboots properly.
	template<typename Inner>
	struct CrashBuffer {
		CrashBuffer() {}

		// If there is a saved crash log, this returns it.
		const char * saved_log() const {
			if (log_data)
				return log_data.data.start_of_message;
			else
				return nullptr;
		}

		size_t saved_log_length() const {
			if (log_data)
				return log_data.data.length_of_message;
			else
				return 0;
		}

		// Destroys the crash log (if any exists).
		void drop_log() {
			memset(underlying_raw, 0, sizeof underlying_raw);
		}

		// Destroys the crash log (if any exists) and initializes the Inner type.
		void init_underlying() {
			drop_log();
			new (&underlying) Inner{};
		}

		// Silently drops the Inner type and begins writing crash data into RAM.
		void start_log() {
			memset(free_message_space, 0, sizeof free_message_space);
			log_data.data = typename LogHeader::LogHeaderData(free_message_space, 0);
		}

		enum WriteStatus {
			WriteOk,
			WriteBufferFull,
			WriteNotStarted
		};

		// Writes data into the crash log, returning a status code about whether it failed.
		WriteStatus write_log(const char *data, size_t length) {
			if (!log_data)
				return WriteNotStarted;
			size_t available_space = max_message_size - log_data.data.length_of_message;
			size_t new_length = length > available_space ? available_space : length;
			memmove(free_message_space + log_data.data.length_of_message, data, new_length);
			log_data.data = typename LogHeader::LogHeaderData(free_message_space, log_data.data.length_of_message + new_length);
			if (new_length < length)
				return WriteBufferFull;
			else
				return WriteOk;
		}

		// Returns a region of memory that can be used safely for constructing log messages.
		std::pair<char *, size_t> log_scratch_buffer() const {
			if (!log_data)
				return {nullptr, 0};

			return std::make_pair(
					free_message_space + log_data.data.length_of_message,
					max_message_size - log_data.data.length_of_message
			);
		}

		auto& operator*() { return underlying; }
		const auto& operator*() const { return underlying; }

		auto* operator->() { return &underlying; }
		const auto* operator->() const { return &underlying; }
	private:
		union LogHeader {
			intptr_t raw_data[3];
			struct LogHeaderData {
				const char * start_of_message;
				size_t       length_of_message;
				intptr_t     checksum;

				LogHeaderData(const char *som, size_t lom) :
					start_of_message(som),
					length_of_message(lom),
					checksum((intptr_t)0x0badc4a45 - (intptr_t)som - (intptr_t)lom) {}
			} data;

			operator bool() const {
				return raw_data[0] + raw_data[1] + raw_data[2] == 0x0badc4a45;
			}
		};

		const static inline size_t max_message_size = sizeof(Inner) - sizeof(LogHeader);

		union {
			struct {
				LogHeader log_data;
				mutable char free_message_space[max_message_size];
			};
			Inner underlying;
			alignas(Inner) char underlying_raw[sizeof(Inner)];
		};
	};
}
