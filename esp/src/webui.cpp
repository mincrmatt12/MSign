#include "webui.h"
#include <SdFat.h>
#include "config.h"
#include "serial.h"
#include "common/util.h"
#include "util.h"
extern "C" {
#include "parser/http_serve.h"
}
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <libb64/cencode.h>

// routines for loading the sd card content
extern SdFatSoftSpi<D6, D2, D5> sd;

// config defaults
#define DEFAULT_USERNAME "admin"
#define DEFAULT_PASSWORD "admin"

// sim stuff
#ifndef strcasecmp_P
#define strcasecmp_P strcasecmp
#endif

namespace webui {

	// webserver
	char * etags[3];

	// server
	WiFiServer webserver(80);
	WiFiClient activeClient;

	// client state
	enum struct ClientState {
		RX_HEADER,
		RX_BODY,
		SENDING_BODY
	} cstate;

	http_serve_state_t * reqstate = nullptr;

	bool _doauth(const char *in) {
		const char * user = config::manager.get_value(config::CONFIG_USER, DEFAULT_USERNAME);
		const char * pass = config::manager.get_value(config::CONFIG_PASS, DEFAULT_PASSWORD);

		char actual_buf[64] = {0};
		char * c = actual_buf;

		base64_encodestate es;
		base64_init_encodestate(&es);

		c += base64_encode_block(user, strlen(user), c, &es);
		c += base64_encode_block(":", 1, c, &es);
		c += base64_encode_block(pass, strlen(pass), c, &es);
		c += base64_encode_blockend(c, &es);
		*c = 0;

		return (strncmp(in, actual_buf, 64) == 0);
	}

	void init() {
		// check if an etag file exists on disk (deleted during updates)
		if (!sd.exists("/web/etag.txt")) {
			char etag_buffer[16] = {0};
			auto etag_num = secureRandom(ESP.getCycleCount() + ESP.getChipId());
			snprintf(etag_buffer, 16, PSTR("E%9ldjs"), etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("E%9ldcs"), etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("E%9ldht"), etag_num);
			etags[2] = strdup(etag_buffer);

			File fl = sd.open("/web/etag.txt", O_CREAT | O_TRUNC | O_WRITE);
			fl.print(etag_num);
			fl.close();
		}
		else {
			File fl = sd.open("/web/etag.txt", FILE_READ);
			char etag_buffer[16] = {0};
			auto etag_num = fl.parseInt();
			fl.close();

			snprintf(etag_buffer, 16, PSTR("E%9ldjs"), etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("E%9ldcs"), etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("E%9ldht"), etag_num);
			etags[2] = strdup(etag_buffer);
		}

		// Start the webserver
		webserver.begin();
	}
	
	template<typename HdrT=char>
	void send_static_response(int code, const char * code_msg, const char * pstr_msg, const HdrT * extra_hdrs=nullptr) {
		int length = strlen_P(pstr_msg);

		activeClient.printf_P(PSTR("HTTP/1.1 %d "), code);
		activeClient.print(FPSTR(code_msg));
		if (extra_hdrs) {
			activeClient.print("\r\n");
			activeClient.print(extra_hdrs);
		}
		activeClient.printf_P(PSTR("\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n"), length);
		activeClient.print(FPSTR(pstr_msg));
	}

	void do_api_response(const char * tgt) {
		// Temp.
		send_static_response(200, PSTR("OK"), PSTR("ok."));
	}

	// Send this file as a response
	void send_cacheable_file(const char * filename_pstr) {
		char filename[32];
		strncpy_P(filename, filename_pstr, 32);
		char real_filename[32];

		if (reqstate->c.is_gzipable) {
			snprintf_P(real_filename, 32, PSTR("web/%s.gz"), filename);
			if (!sd.exists(real_filename)) {
				Log.println(F("Missing gzipped copy, falling back to uncompressed"));
				reqstate->c.is_gzipable = false;
			}
		}
		if (!reqstate->c.is_gzipable) {
			snprintf_P(real_filename, 32, PSTR("web/%s"), filename);
			if (!sd.exists(real_filename)) {
				Log.println(F("Missing key file from SD web archive... aborting"));
				return;
			}
		}

		File f = sd.open(real_filename, FILE_READ);

		Log.println(F("File size="));
		Log.println(f.size());
		
		if (reqstate->c.is_gzipable) activeClient.printf_P(PSTR("Content-Encoding: gzip\r\n"));
		activeClient.printf_P(PSTR("Content-Length: %d\r\n\r\n"), f.size());

		char sendbuf[512];

		while (f.available()) {
			int chunkSize = f.available();
			Log.print(F("Sending chunk = "));
			Log.println(chunkSize);
			if (chunkSize > 512) chunkSize = 512;
			f.read(sendbuf, chunkSize);
			activeClient.write(sendbuf, 512);
		}

		f.close();
	}

	void start_real_response() {
		// First we verify the authentication
		switch (reqstate->c.auth_type) {
			case HTTP_SERVE_AUTH_TYPE_NO_AUTH:
				send_static_response(401, PSTR("Unauthorized"), PSTR("No authentication was provided"), F("WWW-Authenticate: Basic realm=\"msign\""));
				return;
			case HTTP_SERVE_AUTH_TYPE_INVALID_TYPE:
				send_static_response(401, PSTR("Unauthorized"), PSTR("Invalid authentication type."));
				return;
			case HTTP_SERVE_AUTH_TYPE_OK:
				if (!_doauth(reqstate->c.auth_string)) {
					send_static_response(403, PSTR("Forbidden"), PSTR("Invalid authentication."));
					return;
				}
				break;
		}

		// Check if this is an API method
		if (strncmp_P(reqstate->c.url, PSTR("a/"), 2) == 0) {
			do_api_response(reqstate->c.url + 2);
		}
		else {
			// Check for a favicon
			if (strcasecmp_P(reqstate->c.url, PSTR("favicon.ico")) == 0) {
				Log.println(F("Sending 404"));
				// Send a 404
				send_static_response(404, PSTR("Not Found"), PSTR("No favicon is present"));
				return;
			}

			const char * valid_etag = nullptr;
			const char * actual_name = nullptr;
			const char * content_type = nullptr;
			
			// Check for the js files
			if (strcasecmp_P(reqstate->c.url, PSTR("page.js")) == 0) {
				valid_etag = etags[0];
				actual_name = PSTR("page.js");
				content_type = PSTR("application/javascript");
			}
			else if (strcasecmp_P(reqstate->c.url, PSTR("page.css")) == 0) {
				valid_etag = etags[1];
				actual_name = PSTR("page.css");
				content_type = PSTR("text/css");
			}
			else {
				valid_etag = etags[2];
				actual_name = PSTR("page.html");
				content_type = PSTR("text/html");
			}

			// Check for etag-ability
			if (reqstate->c.is_conditional && strcmp(valid_etag, reqstate->c.etag) == 0) {
				// Send a 304
				activeClient.print(F("HTTP/1.1 304 Not Modified\r\nCache-Control: max-age=172800, stale-while-revalidate=604800\r\nConnection: close\r\nETag: "));
				activeClient.print(valid_etag);
				activeClient.print(F("\r\n\r\n"));
			}
			else {
				Log.println(F("Starting response"));
				// Start a response
				activeClient.print(F("HTTP/1.1 200 OK\r\nCache-Control: max-age=172800, stale-while-revalidate=604800\r\nConnection: close\r\nContent-Type: "));

				// Send the content type
				activeClient.print(FPSTR(content_type));
				// Send the etag
				activeClient.printf_P(PSTR("\r\nETag: \"%s\"\r\n"), valid_etag);

				// Send the file contents
				send_cacheable_file(actual_name);
			}
		}
	}
	
	void start_response() {
		Log.println(F("Got request from server"));
		switch (reqstate->c.error_code) {
			case HTTP_SERVE_ERROR_CODE_OK:
				start_real_response();
				break;
			case HTTP_SERVE_ERROR_CODE_BAD_METHOD:
				send_static_response(501, PSTR("Not Implemented"), PSTR("This server only implements GET and POST requests"));
				break;
			case HTTP_SERVE_ERROR_CODE_UNSUPPORTED_VERSION:
				send_static_response(505, PSTR("HTTP Version Not Supported"), PSTR("This server only supports versions 1.0 and 1.1"));
				break;
			case HTTP_SERVE_ERROR_CODE_URL_TOO_LONG:
				// Send a boring old request
				send_static_response(414, PSTR("URI Too Long"), PSTR("The URI specified was too long."));
				break;
			case HTTP_SERVE_ERROR_CODE_BAD_REQUEST:
				// Send a boring old request
				send_static_response(400, PSTR("Bad Request"), PSTR("You have sent an invalid request"));
				break;
			default:
				// Send a boring old request
				send_static_response(500, PSTR("Internal Server Error"), PSTR("The server was unable to interpret your request for an unspecified reason."));
				break;
		}
		activeClient.stop();
	}

	void loop() {
		if (activeClient) {
			// Deal with the client
rerun:
			switch (cstate) {
				case ClientState::RX_HEADER:
					while (activeClient.available()) {
						switch (http_serve_feed(activeClient.read(), false, reqstate)) {
							case HTTP_SERVE_FAIL:
								reqstate->c.error_code = HTTP_SERVE_ERROR_CODE_BAD_REQUEST;
								[[fallthrough]];
							case HTTP_SERVE_DONE:
								// Done handling
								start_response();
							default:
								break;
						}
					}
					break;
				default:
					activeClient.stop();
					break;
			}
		}
		else {
			activeClient = webserver.available();
			if (activeClient) {
				cstate = ClientState::RX_HEADER;
				reqstate = new http_serve_state_t;
				http_serve_start(reqstate);
				goto rerun;
			}
			else {
				if (reqstate) {
					delete reqstate;
					reqstate = nullptr;
				}
			}
		}
	}
}
