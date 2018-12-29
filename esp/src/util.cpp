#include "util.h"
#include "string.h"

util::Download util::download_from(HttpClient &client, const char *host, const char *path, uint32_t timeout) {
	if (client.get(host, path) != 0) {
		return {
			0,
			nullptr,
			0,
			true
		};
	}

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
