#include "util.h"
#include <string.h>

// shitty HTTP client....
const char *msign_ua = "MSign/4.0.0 ESP8266 screwanalytics/1.0";

struct HttpAdapter;
struct HttpsAdapter;

template<typename T>
inline util::Download download_from_impl(const char *host, const char *path, const char * const headers[][2], const char * method, const char * body) {
	return {};
}

util::Download util::download_from(const char *host, const char *path, const char * const headers[][2], const char * method, const char * body) {
	if (host[0] != '_') {
		return ::download_from_impl<HttpAdapter>(host, path, headers, method, body);
	}
	else {
		return ::download_from_impl<HttpsAdapter>(++host, path, headers, method, body);
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
	/*static auto& dwnld = get_downloader<T>();
	dwnld.request(host, path, method, headers, body);

	status_code_out = dwnld.response_code;
	size_out = dwnld.response_size;

	return dwnld.next; */
	return {};
}

std::function<int16_t (void)> util::download_with_callback(const char * host, const char * path, const char * const headers[][2], const char * method, const char * body, int16_t &status_code_out, int32_t &size_out) {
	if (host[0] != '_') {
		return ::download_with_callback_impl<HttpAdapter>(host, path, headers, method, body, status_code_out, size_out);
	}
	else {
		return ::download_with_callback_impl<HttpsAdapter>(++host, path, headers, method, body, status_code_out, size_out);
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
}
