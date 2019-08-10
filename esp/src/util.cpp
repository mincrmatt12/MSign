#include "util.h"
#include "string.h"
#include "WiFiClient.h"

// shitty HTTP client....
const char * msign_ua = "MSign/2.1.0 ESP8266 screwanalytics/1.0";

struct Downloader {
	WiFiClient cl;
	uint16_t response_code;
	int32_t  response_size;

	// send a request
	bool request(const char *host, const char *path, const char* method, const char * const headers[][2], const char * body=nullptr) {
		if (cl.connected()) cl.stop();
		response_size = -1;
		response_code = 0;
		// connect to the server
		if (!cl.connect(host, 80)) return false;

		// send the request
		cl.write(method);
		cl.write(' ');
		cl.write(path);
		cl.write(" HTTP/1.0\r\n");

		// send the host header
		write_header("Host", host);
		// send the user agent header
		write_header("User-Agent", msign_ua);
		// if we're sending a body send the length
		if (body) {
			size_t size = strlen(body);
			char buf[20];
			ultoa(size, buf, 10);
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
			if (millis() - to_start > 500) {
				cl.stop();
				return false;
			}
		}
		if (!cl.read(buf, 4)) {
			cl.stop();
			return false;
		}

		if (memcmp(buf, "HTTP", 4) != 0) {
			cl.stop();
			return false;
		}

		cl.setTimeout(500);

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

		// now, consume all the headers.
		to_start = millis();
		while (true) {
			while (!cl.available()) {
				delay(5);
				if (millis() - to_start > 500) {
					cl.stop();
					return false;
				}
			}
			char starting = cl.read();
			if (starting != '\r' || starting != '\n') {
				if (!cl.find('\n')) {
					cl.stop();
					return false;
				}
			}
			else {
				if (starting == '\r') {
					while (!cl.available()) {
						delay(5);
						if (millis() - to_start > 500) {
							cl.stop();
							return false;
						}
					}
					cl.read();
				}
				if (starting == 'C') {
					// could be the content-length
					while (cl.available() < 2) {
						delay(5);
						if (millis() - to_start > 500) {
							cl.stop();
							return false;
						}
					}
					if (cl.read() == 'o') {
						if (cl.read() == 'n') {
							// wait for the space
							if (!cl.find(' ')) {
								cl.stop();
								return false;
							}

							// read an integer
							response_size = cl.parseInt();

							// read another newline
							if (!cl.find('\n')) {
								cl.stop();
								return false;
							}
						}
					}
				}
				break;
			}
		}

		// at this point we are after all the headers (two newlines found w/o another character before then
		return true;
	}

	void close() {
		if (cl.connected()) cl.stop();
	}

	int16_t next() {
		if (!cl.connected()) return -1;
		else {
			auto to_start = millis();
			while (!cl.available()) {
				delay(5);
				if (millis() - to_start > 20) {
					cl.stop();
					return -1;
				}
			}
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
} dwnld;

util::Download util::download_from(const char *host, const char *path, const char * const headers[][2], const char * method, const char * body) {
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

util::Download util::download_from(const char *host, const char *path) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_from(host, path, headers);
}

util::Download util::download_from(const char * host, const char * path, const char * const headers[][2]) {
	return download_from(host, path, headers, "GET");
}


std::function<char (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body, int16_t &status_code_out, int32_t &size_out) {
	dwnld.request(host, path, method, headers, body);

	status_code_out = dwnld.response_code;
	size_out = dwnld.response_size;

	return []() -> char {
		auto x = dwnld.next();
		if (x == -1) return 0;
		return x;
	};
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
}
