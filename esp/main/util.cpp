#include "util.h"
#include "esp_log.h"
#include <cstdlib>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <netdb.h>
extern "C" {
#include <http_client.h>
};

#undef connect
#undef read
#undef write
#undef socket
#undef send
#undef recv
#undef close
#undef bind

// shitty HTTP client....
const char *msign_ua = "MSign/4.0.0 ESP8266 screwanalytics/1.0";
static const char *TAG = "utildownload";

namespace util {
	namespace detail {
		const static char * TAG = "d_impl";

		namespace adapter {
			const static char * TAG = "d_adapter";
			struct HttpAdapter {
				bool connect(const char *host, const char *port="80") {
					if (is_connected()) return false;

					addrinfo hints{};
					addrinfo *result{}, *rp{};
					hints.ai_family = AF_INET;
					hints.ai_socktype = SOCK_STREAM;

					int stat;
					if ((stat = lwip_getaddrinfo(host, port, &hints, &result))) {
						ESP_LOGE(TAG, "gai fail: %s", lwip_strerr(stat));
					}

					for (rp = result; rp != nullptr; rp = rp->ai_next) {
						sockno = lwip_socket(rp->ai_family, rp->ai_socktype,
								rp->ai_protocol);
						if (sockno == -1)
							continue;

						if (lwip_connect(sockno, rp->ai_addr, rp->ai_addrlen) != -1)
							break;                  /* Success */

						lwip_close(sockno);
						sockno = -1;
					}

					lwip_freeaddrinfo(result);

					if (rp == nullptr) {
						ESP_LOGE(TAG, "failed to lookup %s", host);
						return false;
					}

					// Set a 5 second rcvtimeo 
					const struct timeval timeout = { 5, 0 };
					setsockopt(sockno, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

					// We are now connected.
					return true;
				}

				size_t read(uint8_t *buf, size_t max) {
					int code = lwip_recv(sockno, buf, max, 0);
					if (code >= 0) return code;
					else {
						switch (errno) {
							default:
								lwip_close(sockno);
							case EBADF:
								sockno = -1;
								return -1;
						}
					}
				}
				bool write(const uint8_t *buf, size_t length) {
					int code = lwip_send(sockno, buf, length, 0);
					if (code >= 0) return true;
					else {
						switch (errno) {
							default:
								lwip_close(sockno);
							case EBADF:
								sockno = -1;
								return false;
						}
					}
				}
				bool write(const char *buf) {
					return write((const uint8_t *)buf, strlen(buf));
				}

				void close() {
					if (sockno != -1) lwip_close(sockno);
					sockno = -1;
				}

				bool is_connected() {
					return sockno != -1;
				}
				private:
				int sockno = -1;
			};

			struct HttpsAdapter : HttpAdapter {
			};
		}

		template<typename Adapter>
		struct Downloader {
			// helper for char-by-char access
			//
			// will this perform terribly? perhaps!
			int16_t next() {
				uint8_t buf;
				if (!read_from(&buf, 1)) return -1;
				return buf;
			}

			size_t read_from(uint8_t *buf, size_t max) {
				if (!socket.is_connected()) return 0;
				int r = socket.read(buf, max);
				if (r < 0) {
					ESP_LOGW(TAG, "Read failed.");
					socket.close();
					return 0;
				}
				return r;
			}

			// Close sockets
			void stop() {
				socket.close();
			}

			inline const int& result_code() const {
				return state.c.result_code;
			}

			inline const int& content_length() const {
				return state.c.content_length;
			}

			inline bool is_unknown_length() const {
				return state.c.connection == HTTP_CLIENT_CONNECTION_CLOSE;
			}

			bool request(const char *host, const char *path, const char* method, const char * const headers[][2], const uint8_t * body=nullptr, const size_t bodylen=0) {
				if (socket.is_connected()) {
					ESP_LOGW(TAG, "Socket wasn't closed, closing");
					socket.close();
				}
				if (!socket.connect(host)) {
					ESP_LOGE(TAG, "Failed to connect to host");
					return false;
				}
				
				// Send request
				if (!(socket.write(method) && 
					socket.write(" ") &&
					socket.write(path) &&
					socket.write(" HTTP/1.1\r\n"))) {
					ESP_LOGE(TAG, "Failed to send request path");

					socket.close();
					return false;
				}

				// Send headers
				if (!(write_header("Host", host) &&
					  write_header("User-Agent", msign_ua))) {

					ESP_LOGE(TAG, "Failed to send request headers");

					socket.close();
					return false;
				}

				// Send body length if present
				if (body) {
					char buf[32];
					snprintf(buf, 32, "%d", (int)bodylen);
					if (!write_header("Content-Length", buf)) {
						ESP_LOGE(TAG, "Failed to send body headers");

						socket.close();
						return false;
					}
				}

				// Write all provided headers
				for (int i = 0;;++i) {
					if (headers[i][0] == nullptr || headers[i][1] == nullptr) break;
					if (!write_header(headers[i][0], headers[i][1])) {
						ESP_LOGE(TAG, "Failed to send user headers");

						socket.close();
						return false;
					}
					ESP_LOGD(TAG, "hdr: %s --> %s", headers[i][0], headers[i][1]);
				}

				socket.write("\r\n");
				if (body) {
					if (!socket.write(body, bodylen)) {
						ESP_LOGE(TAG, "Failed to send body");

						socket.close();
						return false;
					}
				}

				// Initialize http request parser
				http_client_start(&state);

				while (true) {
					uint8_t buf;
					int recvd_bytes = socket.read(&buf, 1);
					if (recvd_bytes < 0) {
						ESP_LOGE(TAG, "Got error while recving");
						socket.close();
						return false;
					}
					// Otherwise, feed it into nmfu
					switch (http_client_feed(buf, false, &state)) {
						case HTTP_CLIENT_OK:
							continue;
						case HTTP_CLIENT_FAIL:
							ESP_LOGE(TAG, "Parser failed");
							socket.close();
							return false;
						case HTTP_CLIENT_DONE:
							ESP_LOGD(TAG, "Finished parsing req headers");
							goto finish_req;
					}
				}
finish_req:
				ESP_LOGD(TAG, "Ready with code = %d; length = %d; unklen = %d", result_code(), content_length(), is_unknown_length());
				return true;
			}
		private:
			Adapter socket;
			http_client_state_t state;

			bool write_header(const char * name, const char * value) {
				if (!socket.write(name)) return false;
				if (!socket.write(": ")) return false;
				if (!socket.write(value)) return false;
				if (!socket.write("\r\n")) return false;
				return true;
			}
		};

		Downloader<adapter::HttpAdapter> dwnld;
		Downloader<adapter::HttpsAdapter> dwnld_s;

		template<typename T>
		inline constexpr Downloader<T>& get_downloader() {return dwnld;}; 

		template<>
		inline constexpr Downloader<adapter::HttpsAdapter>& get_downloader() {return dwnld_s;};

	}
}

template<typename T>
inline util::Download download_from_impl(const char *host, const char *path, const char * const headers[][2], const char * method, const char * body) {
	constexpr static util::detail::Downloader<T>& dwnld = util::detail::get_downloader<T>();
	util::Download d;
	if (!dwnld.request(host, path, method, headers, (const uint8_t *)body, body ? strlen(body) : 0)) {
		d.error = true;
		return d;
	}

	d.status_code = dwnld.result_code();
	d.error = false;
	if (dwnld.result_code() < 200 || dwnld.result_code() >= 300) {
		dwnld.stop();
		ESP_LOGE(TAG, "download_from_impl got result code %d", d.status_code);
		d.error = true;
		return d;
	}

	if (!dwnld.is_unknown_length()) {
		d.length = dwnld.content_length();
		d.buf = (char *)malloc(d.length);
		// TODO: rewrite me please!
		for (int32_t i = 0; i < d.length; ++i) {
			auto x = dwnld.next();
			if (x != -1) {
				d.buf[i] = (char)x;
			}
			else {
				free(d.buf);
				d.error = true;
				return d;
			}
		}
	}
	else {
		// download until the connection ends
		d.length = 128;
		d.buf = (char *)malloc(d.length);
		size_t i = 0;
		int16_t x;
		while ((x = dwnld.next()) != -1) {
			if (i == d.length) {
				d.length += 128;
				d.buf = (char *)realloc(d.buf, d.length);
				if (d.buf == nullptr) {
					d.error = true;
					return d;
				}
			}
			d.buf[i++] = x;
		}
		d.buf = (char *)realloc(d.buf, i);
		d.length = i;
	}

	return d;
}

util::Download util::download_from(const char *host, const char *path, const char * const headers[][2], const char * method, const char * body) {
	if (host[0] != '_') {
		return ::download_from_impl<detail::adapter::HttpAdapter>(host, path, headers, method, body);
	}
	else {
		return ::download_from_impl<detail::adapter::HttpsAdapter>(++host, path, headers, method, body);
	}
}

util::Download util::download_from(const char *host, const char *path) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_from(host, path, headers);
}

util::Download util::download_from(const char * host, const char * path, const char * const headers[][2]) {
	return download_from(host, path, headers, "GET");
}


template<typename T>
inline std::function<int16_t (void)> download_with_callback_impl(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body, int16_t &status_code_out, int32_t &size_out) {
	static auto& dwnld = util::detail::get_downloader<T>();
	dwnld.request(host, path, method, headers, (const uint8_t *)body, body ? strlen(body) : 0);

	status_code_out = dwnld.result_code();
	size_out = dwnld.content_length();

	return std::bind(&util::detail::Downloader<T>::next, &dwnld);
}

std::function<int16_t (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body, int16_t &status_code_out, int32_t &size_out) {
	if (host[0] != '_') {
		return ::download_with_callback_impl<detail::adapter::HttpAdapter>(host, path, headers, method, body, status_code_out, size_out);
	}
	else {
		return ::download_with_callback_impl<detail::adapter::HttpsAdapter>(++host, path, headers, method, body, status_code_out, size_out);
	}
}

std::function<int16_t (void)> util::download_with_callback(const char * host, const char * path) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_with_callback(host, path, headers);
}
std::function<int16_t (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2]) {
	int16_t st;
	return download_with_callback(host, path, headers, st);
}
std::function<int16_t (void)> util::download_with_callback(const char * host, const char * path, int16_t &status_code_out) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_with_callback(host, path, headers, status_code_out);
}
std::function<int16_t (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], int16_t &status_code_out) {
	int32_t so;
	return download_with_callback(host, path, headers, "GET", nullptr, status_code_out, so);
}
std::function<int16_t (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body) {
	int16_t sco;
	int32_t so;
	return download_with_callback(host, path, headers, method, body, sco, so);
}

void util::stop_download() {
	detail::dwnld.stop();
	detail::dwnld_s.stop();
}
