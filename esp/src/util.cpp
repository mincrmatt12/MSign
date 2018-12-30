#include "util.h"
#include "string.h"
#include "HttpClient.h"
#include "WiFiClient.h"

WiFiClient cl;
HttpClient client(cl);

const char * msign_ua = "MSign/1.0.0 ESP8266";

util::Download util::download_from(const char *host, const char *path, const char * const headers[][2], uint32_t timeout) {
	if (client.startRequest(host, 80, path, HTTP_METHOD_GET, msign_ua) != 0) {
		return {
			0,
			nullptr,
			0,
			true
		};
	}

	for (int i = 0;;++i) {
		if (headers[i][0] == nullptr || headers[i][1] == nullptr) break;
		client.sendHeader(headers[i][0], headers[i][1]);
		Serial1.printf("dwnld: %s, %s --> %s\n", path, headers[i][0], headers[i][1]);
	}

	client.endRequest();

	Download result = {0};
	result.status_code = client.responseStatusCode();

	client.skipResponseHeaders();

	result.length = client.contentLength();
	result.buf = (char *)malloc(result.length);

	Serial1.printf("dwnld: %s l: %d, s_code: %d\n", path, result.length, result.status_code);
	
	if (result.buf == nullptr) {
		Serial1.printf("dwnld: %s, oom\n", path);
		result.error = true;
		return result;
	}

	char * pos = result.buf;
	uint64_t ts = millis();
	uint64_t read_so_far = 0;

	while ((client.connected() || client.available() || read_so_far < result.length) && (millis() - ts) < timeout) {
		if (client.available()) {
			*pos++ = client.read();
			++read_so_far;
			ts = millis();
		}
		else {
			delay(5);
		}
	}

	client.stop();

	return result;
}

util::Download util::download_from(const char *host, const char *path, uint32_t timeout) {
	const char * const headers[][2] = {{nullptr, nullptr}};
	return download_from(host, path, headers, timeout);
}
