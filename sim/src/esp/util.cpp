#include "util.h"
#include <string.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Time.h>
#include "SdFat.h"

// shitty HTTP client....
const char * msign_ua = "MSign/2.1.0 ESP8266 screwanalytics/1.0";

#define TO_C if ((millis() - to_start) > Adapter::timeout) { cl.stop(); Serial1.println(F("tn: "));Serial1.println(__LINE__); return false; } 
extern SdFatSoftSpi<D6, D2, D5> sd;

struct HttpAdapter {
	typedef WiFiClient Client;
	static const int timeout = 600;

	bool connect(Client& c, const char* host) {
		return c.connect(host, 80);
	}
};

struct HttpsAdapter {
	typedef BearSSL::WiFiClientSecure Client;
	static const int timeout = 3000;

	bool connect(Client& c, const char * host) {
		if (now() < 1000) {
			return false;
		}
		c.setInsecure();
		return c.connect(host, 443);
	}
};

template<typename Adapter>
struct Downloader {
	typename Adapter::Client cl;
	Adapter ad;
	uint16_t response_code;
	int32_t  response_size;
	int32_t i = 0;

	// send a request
	bool request(const char *host, const char *path, const char* method, const char * const headers[][2], const char * body=nullptr) {
		if (cl.connected()) cl.stop();
		response_size = -1;
		response_code = 0;
		i = 0;
		// connect to the server
		if (!ad.connect(cl, host)) return false;
		Serial1.printf_P(PSTR("dwndl,req: %s %s\n"), host, path);

		// send the request
		cl.write(method);
		cl.print(' ');
		cl.write(path);
		cl.write(" HTTP/1.1\r\n");

		// send the host header
		write_header("Host", host);
		// send the user agent header
		write_header("User-Agent", msign_ua);
		// if we're sending a body send the length
		if (body) {
			size_t size = strlen(body);
			char buf[20];
			ltoa(size, buf, 10);
			write_header("Content-Length", buf);
			Serial1.printf("dwnld: bodys %d\n", (int)size);
		}

		// send the headers
		for (int i = 0;;++i) {
			if (headers[i][0] == nullptr || headers[i][1] == nullptr) break;
			write_header(headers[i][0], headers[i][1]);
			Serial1.printf("dwnld: %s, %s --> %s\n", path, headers[i][0], headers[i][1]);
		}

		Serial1.printf("dwnld: %s %s\n", method, path);

		// send blank line
		cl.write("\r\n");

		// send response body, if any
		if (body) {
			cl.write(body);
		}

		// check if the server sends us an http
		uint8_t buf[4];
		auto to_start = millis();

		while (cl.available() < 4) {
			delay(5);
			TO_C;
		}

		if (!cl.read(buf, 4)) {
			cl.stop();
			return false;
		}

		if (memcmp(buf, "HTTP", 4) != 0) {
			cl.stop();
			return false;
		}

		Serial1.println(F("dbgreq got h"));

		cl.setTimeout(Adapter::timeout);

		// alright, let's read till we hit the space
		if (!cl.find(' ')) {
			cl.stop();
			return false;
		}
		response_code = cl.parseInt();
		if (!cl.find('\n')) {
			cl.stop();
			return false;
		}

		Serial1.print(F("dbgreq got hcode: "));
		Serial1.println(response_code);

		// now, consume all the headers.
		to_start = millis();
		while (true) {
			Serial1.println(F("hloop debgrq"));
			while (!cl.available()) {
				delay(5);
				TO_C;
			}
			char starting = cl.read();
			Serial1.println(starting);
			if (starting == '\n') {
				break;
			}
			else {
				if (starting == '\r') {
					while (!cl.available()) {
						delay(5);
						TO_C;
					}
					cl.read();
					break;
				}
				else if (starting == 'C') {
					// could be the content-length
					char buf[15] = {0};
					if (cl.readBytesUntil(':', buf, 14) != 13) goto skip;
					Serial1.println(buf);
					if (strcmp(buf, "ontent-Length") != 0) goto skip;
					// wait for the space
					if (!cl.find(' ')) {
						Serial1.println(F("f3"));
						cl.stop();
						return false;
					}

					// read an integer
					response_size = cl.parseInt();
					Serial1.println(F("got rlen: "));
					Serial1.println(response_size);

skip:
					// read another newline
					if (!cl.find('\n')) {
						Serial1.println(F("f2"));
						cl.stop();
						return false;
					}
				}
				else {
					if (!cl.find('\n')) {
						Serial1.println(F("f2"));
						cl.stop();
						return false;
					}
				}
			}
		}

		Serial1.print(F("ready: "));
		Serial1.println(response_size);

		// at this point we are after all the headers (two newlines found w/o another character before then
		return true;
	}

	void close() {
		cl.stop();
	}

	int16_t next() {
		if (!cl.connected() && !cl.available()) return -1;
		else {
			auto to_start = millis();
			while (!cl.available()) {
				delay(5);
				if (millis() - to_start > Adapter::timeout) {
					cl.stop();
					Serial1.println(F("tnu"));
					return -1;
				}
			}
			++i;
			return cl.read();
		}
	}

private:
	void write_header(const char * name, const char * value) {
		cl.write(name);
		cl.write(": ");
		cl.write(value);
		cl.write("\r\n");
	}
};

Downloader<HttpAdapter> dwnld;
Downloader<HttpsAdapter> dwnld_s;

template<typename T, Downloader<T>& dwnld>
inline util::Download download_from_impl(const char *host, const char *path, const char * const headers[][2], const char * method, const char * body) {
	dwnld.request(host, path, method, headers, body);
	
	util::Download d;
	d.status_code = dwnld.response_code;
	d.error = false;
	if (dwnld.response_code < 200 || dwnld.response_code >= 300) {
		dwnld.close();
		Serial1.printf("dwlnd: got code %d\n", dwnld.response_code);
		d.error = true;
		return d;
	}

	if (dwnld.response_size >= 0) {
		d.length = dwnld.response_size;
		d.buf = (char *)malloc(d.length);
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
		return ::download_from_impl<HttpAdapter, dwnld>(host, path, headers, method, body);
	}
	else {
		return ::download_from_impl<HttpsAdapter, dwnld_s>(++host, path, headers, method, body);
	}
}

util::Download util::download_from(const char *host, const char *path) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_from(host, path, headers);
}

util::Download util::download_from(const char * host, const char * path, const char * const headers[][2]) {
	return download_from(host, path, headers, "GET");
}


template<typename T, Downloader<T>& dwnld>
inline std::function<char (void)> download_with_callback_impl(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body, int16_t &status_code_out, int32_t &size_out) {
	dwnld.request(host, path, method, headers, body);

	status_code_out = dwnld.response_code;
	size_out = dwnld.response_size;

	return []() -> char {
		auto x = dwnld.next();
		if (x == -1) return 0;
		return x;
	};
}

std::function<char (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body, int16_t &status_code_out, int32_t &size_out) {
	if (host[0] != '_') {
		return ::download_with_callback_impl<HttpAdapter, dwnld>(host, path, headers, method, body, status_code_out, size_out);
	}
	else {
		return ::download_with_callback_impl<HttpsAdapter, dwnld_s>(++host, path, headers, method, body, status_code_out, size_out);
	}
}

std::function<char (void)> util::download_with_callback(const char * host, const char * path) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_with_callback(host, path, headers);
}
std::function<char (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2]) {
	int16_t st;
	return download_with_callback(host, path, headers, st);
}
std::function<char (void)> util::download_with_callback(const char * host, const char * path, int16_t &status_code_out) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_with_callback(host, path, headers, status_code_out);
}
std::function<char (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], int16_t &status_code_out) {
	int32_t so;
	return download_with_callback(host, path, headers, "GET", nullptr, status_code_out, so);
}
std::function<char (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body) {
	int16_t sco;
	int32_t so;
	return download_with_callback(host, path, headers, method, body, sco, so);
}

void util::stop_download() {
	dwnld.close();
	dwnld_s.close();
}

uint16_t util::compute_crc(uint8_t * buf, size_t len, uint16_t crc) {
	// CRC16:
	// Iterate over all bits, whenever MSB (16-bit segment) is 1 XOR with polynomial
	
	for (size_t index = 0; index < len; ++index) {
		crc ^= buf[index];
		for (uint8_t i = 8; i < 9; --i) { // count backwards
			// Shift crc
			crc = (crc >> 1) ^ ((crc & 1) ? 0x8005 : 0);
		}
	}

	return crc;
}

bool util::crc_valid(uint8_t * buf, size_t len) {
	return compute_crc(buf, len-2) == *(uint16_t *)(buf + (len - 2));
}

util::LogClass Log;

size_t util::LogClass::write(uint8_t c) {
	if (!quiet_mode) Serial1.write(c);

	// Buffer to SD card
	_put(c);
}

void util::LogClass::update_logs() {
	if (_remainBuf() < 250) return;
	File f;
	if (bytes_sent_to_log > MAX_LOG_FILE_SIZE) {
		f = sd.open("/log.txt", O_TRUNC | O_WRITE);
		bytes_sent_to_log = 0;
	}
	else {
		f = sd.open("/log.txt", O_APPEND | O_WRITE);
	}
	uint8_t buf[200];
	while (_remainBuf() >= 200) {
		_grab(buf, 200);
		f.write(buf, 200);
		f.flush();
	}
	f.close();
}

size_t util::LogClass::_remainBuf() {
	if (start <= end) return end - start;
	else return (&buf[1024] - start) + (end - &buf[0]);
}

void util::LogClass::_put(uint8_t c) {
	if (_remainBuf() == 1024) return;

	*end = c;

	if (end == &buf[1024]) end = buf + 1;
	else ++end;
}

void util::LogClass::_grab(uint8_t * obuf, size_t length) {
	if (length > _remainBuf()) return;

	memcpy(obuf, start, std::min((ptrdiff_t)length, &buf[1024] - start));
	if (length > &buf[1024] - start) {
		memcpy(obuf + (&buf[1024] - start), buf, length - (&buf[1024] - start));
		start = &buf[length - (&buf[1024] - start)];
	}
	else {
		start += length;
	}
}
