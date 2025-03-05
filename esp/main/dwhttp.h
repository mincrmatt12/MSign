#ifndef MSE_UTIL_H
#define MSE_UTIL_H

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <utility>

#define MAX_LOG_FILE_SIZE 64 * 1024 * 1024

namespace dwhttp {
	namespace detail {
		// Internal type which hides HTTP/HTTPS connections
		struct DownloaderBase;
	}

	// Simplified interface to Connection which doesn't allow dynamically providing
	// headers / body.
	struct Download {
		Download(const Download&) = delete;
		Download(Download&& other) : recvpending(other.recvpending) {
			adapter = std::exchange(other.adapter, nullptr);
			recvbuf = std::exchange(other.recvbuf, nullptr);
		}
		Download& operator=(const Download&) = delete;
		Download& operator=(Download&& other) {
			if (this != &other) {
				this->~Download();
				adapter = std::exchange(other.adapter, nullptr);
				recvbuf = std::exchange(other.recvbuf, nullptr);
			}
			return *this;
		}
		~Download();

		// Internal use only.
		Download(detail::DownloaderBase * adapter) : adapter(adapter) {}
		
		// Read single char
		int16_t operator()();
		size_t operator()(uint8_t * buf, size_t length);

		int result_code() const;
		int content_length() const;
		bool is_unknown_length() const;

		void make_nonclose() {nonclose = true;}
		bool ok() const {return result_code() >= 200 && result_code() < 300;}

		void stop();
	protected:
		uint8_t * recvbuf {};

		uint8_t recvpending{};
		uint8_t recvread{};

		uint8_t nonclose = false;

		detail::DownloaderBase * adapter{};
	};

	struct Connection : Download {
		enum State {
			SEND_HEADERS,
			SEND_BODY,
			RECEIVE,
			CLOSED
		};

		using Download::Download;

		State state() const {
			return current_state;
		}

		bool ok() const {
			return current_state < RECEIVE || Download::ok();
		}

		void send_header_name(const char *name);
		void send_header_value_part(const char *value, size_t length);
		void send_header_value_part(const char *value) {
			send_header_value_part(value, strlen(value));
		}
		void send_header_end();

		void send_header(const char *name, const char* value) {
			send_header_name(name);
			send_header_value_part(value);
			send_header_end();
		}

		class HeaderHelper {
			Connection* parent{};
			friend Connection;

			HeaderHelper(Connection *parent) : parent(parent) {}
		public:
			HeaderHelper(const HeaderHelper& other) = delete;
			HeaderHelper(HeaderHelper&& other) : parent(std::exchange(other.parent, nullptr)) {};
			HeaderHelper& operator=(const HeaderHelper& other) = delete;
			HeaderHelper& operator=(HeaderHelper&& other) {
				if (&other == this) return *this;
				this->~HeaderHelper();
				parent = std::exchange(other.parent, nullptr);
				return *this;
			}
			~HeaderHelper() {
				if (parent) parent->send_header_end();
			}

			void operator+=(const char* value) const {
				return parent->send_header_value_part(value);
			}
			void send(const char *value, size_t length) const {
				return parent->send_header_value_part(value, length);
			}
			void send(const char *value) const {
				return parent->send_header_value_part(value);
			}
		};

		HeaderHelper begin_header(const char* name) {
			send_header_name(name);
			return {this};
		}

		void start_body(size_t content_length); // unchunked
		void start_body();                      // chunked

		void write(const char* body) {
			return write((const uint8_t *)body, strlen(body));
		}
		void write(const uint8_t* body, size_t length);

		void end_body();
	private:
		State current_state = SEND_HEADERS;
		bool transfer_chunked = false;
	};

	Download download_with_callback(const char * host, const char * path);
	Download download_with_callback(const char * host, const char * path, const char * const headers[][2]);
	Download download_with_callback(const char * host, const char * path, const char * const headers[][2]);
	Download download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body);

	Connection open_site(const char * host, const char * path, const char * method="GET");

	void close_connection(bool ssl);

	size_t url_encoded_size(const char *path, bool ignore_slashes = false); 
	void url_encode(const char *src, char *target, size_t tgtlen, bool ignore_slashes = false);
}

#endif
