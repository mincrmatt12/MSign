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
			snprintf(etag_buffer, 16, PSTR("\"E%9ldjs\""), etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("\"E%9ldcs\""), etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("\"E%9ldht\""), etag_num);
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

			snprintf_P(etag_buffer, 16, PSTR("\"E%9ldjs\""), etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("\"E%9ldcs\""), etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, PSTR("\"E%9ldht\""), etag_num);
			etags[2] = strdup(etag_buffer);
		}

		// Start the webserver
		webserver.begin();
	}
	
	template<typename HdrT=char>
	void send_static_response(int code, const char * code_msg, const char * pstr_msg, const HdrT * extra_hdrs=nullptr) {
		int length = strlen_P(pstr_msg);

		activeClient.printf_P(PSTR("HTTP/1.1 %d "), code);
		activeClient.print(F(code_msg));
		if (extra_hdrs) {
			activeClient.print("\r\n");
			activeClient.print(extra_hdrs);
		}
		activeClient.printf_P(PSTR("\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n"), length);
		activeClient.print(F(pstr_msg));
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

		// Temp.
		send_static_response(200, PSTR("OK"), PSTR("ok."));
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
