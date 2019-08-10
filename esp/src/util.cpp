#include "util.h"
#include "string.h"
#include "WiFiClient.h"

const char * msign_ua = "MSign/2.1.0 ESP8266 screwanalytics/1.0";

struct Downloader {
	WiFiClient cl;
	uint16_t response_code;

	// send a request
	bool request(const char *host, const char *path, const char* method, const char * const headers[][2], const char * body=nullptr) {
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
		}

		// send the headers
		for (int i = 0;;++i) {
			if (headers[i][0] == nullptr || headers[i][1] == nullptr) break;
			write_header(headers[i][0], headers[i][1]);
			Serial1.printf("dwnld: %s, %s --> %s\n", path, headers[i][0], headers[i][1]);
		}

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

util::Download util::download_from(const char *host, const char *path, const char * const headers[][2]) {
	dwnld.request(host, path, "GET", headers);

}

util::Download util::download_from(const char *host, const char *path) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_from(host, path, headers);
}
