#ifndef MSE_UTIL_H
#define MSE_UTIL_H

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <functional>

#define MAX_LOG_FILE_SIZE 64 * 1024 * 1024

namespace dwhttp {
	/* TODO: nothing uses this right now
	struct RawDownload {
		int16_t status_code;
		char * buf;
		uint32_t length;
		bool error;
	};

	// Mallocs a buffer, and downloads.
	// Also occasionally runs the serial buffer.
	// Does not make a c_str, returns the length in a struct however. The response may be less than length if error is set.
	RawDownload download_from(const char * host, const char * path);
	RawDownload download_from(const char * host, const char * path, const char * const headers[][2]);
	RawDownload download_from(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body=nullptr);
	*/

	struct Download {
		Download(const Download&) = delete;
		Download& operator=(const Download&) = delete;
		Download(Download&& other) : recvpending(other.recvpending) {
			adapter = std::exchange(other.adapter, nullptr);
			recvbuf = std::exchange(other.recvbuf, nullptr);
		}
		~Download();

		// Internal use only.
		Download(void * adapter) : adapter(adapter) {}
		
		// Read single char
		int16_t operator()();
		size_t operator()(uint8_t * buf, size_t length);

		int result_code() const;
		int content_length() const;
		bool is_unknown_length() const;

	private:

		uint8_t * recvbuf {};
		uint8_t recvpending{};
		uint8_t recvread{};
		void * adapter{};
	};

	// Starts the downloader, but returns a function that will return a byte (1-255) indicating data or 0 indicating end of transmission. Not designed for use with binary data.
	Download download_with_callback(const char * host, const char * path);
	Download download_with_callback(const char * host, const char * path, const char * const headers[][2]);
	Download download_with_callback(const char * host, const char * path, const char * const headers[][2]);
	Download download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body);
}

#endif
