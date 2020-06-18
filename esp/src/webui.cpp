#include "webui.h"
#include <SdFat.h>
#include "config.h"
#include "serial.h"
#include "common/util.h"
#include "util.h"
extern "C" {
#include "parser/http_serve.h"
#include "parser/multipart_header.h"
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

	http_serve_state_t * reqstate = nullptr;

	bool check_auth(const char *in) {
		const char *user = config::manager.get_value(config::CONFIG_USER, DEFAULT_USERNAME);
		const char *pass = config::manager.get_value(config::CONFIG_PASS, DEFAULT_PASSWORD);

		char actual_buf[64] = {0};
		char *c = actual_buf;

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
			long etag_num = secureRandom(ESP.getCycleCount() + ESP.getChipId());
			snprintf_P(etag_buffer, 16, PSTR("E%9ldjs"), etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf_P(etag_buffer, 16, PSTR("E%9ldcs"), etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf_P(etag_buffer, 16, PSTR("E%9ldht"), etag_num);
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

			snprintf_P(etag_buffer, 16, PSTR("E%9ldjs"), etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf_P(etag_buffer, 16, PSTR("E%9ldcs"), etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf_P(etag_buffer, 16, PSTR("E%9ldht"), etag_num);
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

	void stream_file(File& f) {
		Log.println(F("File size="));
		activeClient.printf_P(PSTR("Content-Length: %lu\r\n\r\n"), f.size());

		char sendbuf[512];

		while (f.available()) {
			int chunkSize = f.available();
			Log.print(F("Sending chunk = "));
			Log.println(chunkSize);
			if (chunkSize > 512) chunkSize = 512;
			f.read(sendbuf, chunkSize);
			activeClient.write(sendbuf, 512);

			// TODO: get this to potentially yield back to the serial processing thread?
		}

		f.close();
	}

	// AD-HOC MULTIPART/FORM-DATA PARSER
	//
	// Does contain an NMFU-based parser for the headers though
	//
	// Structure as a loop of
	//  - wait for \r\n--
	//  - using the helper function to bail early read and compare to the boundary. if at any point we fail go back to step 1 without eating
	//  - start the content-disposition NMFU (this sets up in a state struct which is passed in)
	//  - it eats until the body
	//  - if there's a content length, read that many
	//  - otherwise, read until \r\n-- again
	//
	// Takes a hook with the following "virtual" typedef
	// Size == -1 means start
	//      == -2 means end
	//
	// typedef bool (*MultipartHook)(uint8_t * tgt, int size, multipart_header_state_t * header_state);

	enum struct MultipartStatus {
		OK,
		INVALID_HEADER,
		NO_CONTENT_DISP,
		EOF_EARLY,
		HOOK_ABORT
	};

	int read_from_req_body(uint8_t * tgt, int size) {
		// TODO: make me handle chunked encoding
		while (activeClient.available() <= size) {;}
		return activeClient.read(tgt, size);
	}

	int read_from_req_body() {
		// TODO: make me handle chunked encoding
		while (!activeClient.available()) {;} // TODO: timeout
		return activeClient.read();
	}

	template<typename MultipartHook>
	MultipartStatus do_multipart(MultipartHook hook) {
		multipart_header_state_t header_state;

continuewaiting:
		// Try to read the start of a header block
		while (activeClient) {
			auto val = read_from_req_body();
			if (val == '-') break;
			if (val == '\r') {
				if (read_from_req_body() != '\n') continue;
				if (read_from_req_body() != '-') continue;
				break;
			}
		}		
		Log.println(F("got past -"));
		if (!activeClient) return MultipartStatus::EOF_EARLY;
		if (read_from_req_body() != '-') goto continuewaiting;
		Log.println(F("got past -2"));
		for (int i = 0; i < strlen(reqstate->c.multipart_boundary); ++i) {
			if (read_from_req_body() != reqstate->c.multipart_boundary[i]) goto continuewaiting;
		}

		Log.println(F("Got multipart start"));

		while (activeClient) {
			// Start the multipart_header parser
			multipart_header_start(&header_state);

			// Continue parsing until done
			while (true) {
				if (!activeClient) return MultipartStatus::EOF_EARLY;
				switch (multipart_header_feed(read_from_req_body(), false, &header_state)) {
					case MULTIPART_HEADER_OK:
						continue;
					case MULTIPART_HEADER_FAIL:
						return MultipartStatus::INVALID_HEADER;
					case MULTIPART_HEADER_DONE:
						goto endloop;
				}
				continue;
endloop:
				break;
			}

			Log.println(F("Got multipart header end"));

			// Check if this is the end
			if (header_state.c.is_end) return MultipartStatus::OK;
			// Check if this is a valid header
			if (!header_state.c.ok) return MultipartStatus::NO_CONTENT_DISP;
			// Otherwise, start reading the response

			bool is_skipping = false;
			if (!hook(nullptr, -1, &header_state)) {
				is_skipping = true;
			}

			uint8_t buf[512];
			while (true) {
				if (!activeClient) return MultipartStatus::EOF_EARLY;
				// Try to read
				int pos = 0;
				while (true) {
					if (pos == 512) {
						// Send that buffer into the hook
						if (!is_skipping) {
							if (!hook(buf, pos, &header_state)) return MultipartStatus::HOOK_ABORT;
						}
						pos = 0;
					}
					auto inval = read_from_req_body();
					if (inval == '\r') break;
					else {buf[pos++] = inval;}
				}
				// Send that buffer into the hook
				if (pos && !is_skipping) {
					if (!hook(buf, pos, &header_state)) return MultipartStatus::HOOK_ABORT;
				}
				buf[0] = '\r';
				pos = 1;
				// Try to read the rest of the boundary
				if ((buf[pos++] = read_from_req_body()) != '\n') goto flush_buf;
				if ((buf[pos++] = read_from_req_body()) != '-') goto flush_buf;
				if ((buf[pos++] = read_from_req_body()) != '-') goto flush_buf;
				for (int i = 0; i < strlen(reqstate->c.multipart_boundary); ++i) {
					if ((buf[pos++] = read_from_req_body()) != reqstate->c.multipart_boundary[i]) goto flush_buf;
				}
				// We have read an entire boundary delimiter, break out of this loop
				break;
flush_buf:
				if (!is_skipping) {
					if (!hook(buf, pos, &header_state)) return MultipartStatus::HOOK_ABORT;
				}
			}

			if (!is_skipping) hook(nullptr, -2, &header_state); // it's invalid to error in this case

			Log.println(F("got end"));
			Log.println(is_skipping);
		}
		return MultipartStatus::OK;
	}

	void do_api_response(const char * tgt) {
		if (strcasecmp_P(tgt, PSTR("conf.txt")) == 0) {
			if (reqstate->c.method == HTTP_SERVE_METHOD_GET) {
				File cfl = sd.open("config.txt", FILE_READ);
				activeClient.print(F("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n"));
				stream_file(cfl);
			}
			else if (reqstate->c.method == HTTP_SERVE_METHOD_POST) {
				// Verify there's actually a request body
				if (reqstate->c.content_length == 0) goto badrequest;
				if (reqstate->c.content_length > 2048) {
					// Too long
					send_static_response(413, PSTR("Payload Too Large"), PSTR("The provided config is too large, please create one under 2K"));
					return;
				}
				uint8_t buf[reqstate->c.content_length];
				config::manager.use_new_config((char *)buf, read_from_req_body(buf, reqstate->c.content_length));
				// Send a handy dandy 204
				send_static_response(204, PSTR("No Content"), PSTR(""));
			}
			else goto invmethod;
		}
		else if (strcasecmp_P(tgt, PSTR("mpres.json")) == 0) {
			if (reqstate->c.method != HTTP_SERVE_METHOD_GET) goto invmethod;
			bool m0 = sd.exists("/model.bin");
			bool m1 = sd.exists("/model1.bin");

			char buf[34] = {0};
			snprintf_P(buf, 34, PSTR("{\"m0\":%s,\"m1\":%s}"), m0 ? "true" : "false", m1 ? "true" : "false");

			activeClient.print(F("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n"));
			activeClient.printf_P(PSTR("Content-Length: %d\r\n\r\n"), strlen(buf));
			activeClient.print(buf);
		}
		else if (strcasecmp_P(tgt, PSTR("model.bin")) == 0 || strcasecmp_P(tgt, PSTR("model1.bin")) == 0) {
			if (reqstate->c.method != HTTP_SERVE_METHOD_GET) goto invmethod;
			if (!sd.exists(tgt - 1)) goto notfound;

			File mfl = sd.open(tgt - 1, FILE_READ);
			activeClient.print(F("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/octet-stream\r\n"));
			stream_file(mfl);
		}
		else if (strcasecmp_P(tgt, PSTR("newmodel")) == 0) {
			if (reqstate->c.method != HTTP_SERVE_METHOD_POST) goto invmethod;

			File out_model;
			bool ok = false;
			switch (do_multipart([&](uint8_t * buf, int len, multipart_header_state_t *state){
				if (!state->name_counter) {
					Log.println(F("no name"));
					return false;
				}
				
				if (len == -1) {
					out_model = sd.open((strcasecmp_P(state->c.name, PSTR("model")) == 0) ? "/model.bin" : "/model1.bin", O_WRITE | O_CREAT | O_TRUNC);
					return true;
				}
				else if (len == -2) {
					out_model.flush();
					out_model.close();
					ok = true;
					return true;
				}
				else {
					out_model.write(buf, len);
					return true;
				}
			})) {
				case MultipartStatus::EOF_EARLY:
					return;
				case MultipartStatus::INVALID_HEADER:
					send_static_response(400, PSTR("Bad Request"), PSTR("The server was unable to interpret the header area of the form data request."));
					return;
				case MultipartStatus::HOOK_ABORT:
					send_static_response(400, PSTR("Bad Request"), PSTR("You have sent invalid files."));
					return;
				case MultipartStatus::NO_CONTENT_DISP:
					send_static_response(400, PSTR("Bad Request"), PSTR("You are missing a Content-Disposition header in your request."));
					return;
				case MultipartStatus::OK:
					break;
				default:
					return;
			}

			if (!ok) send_static_response(400, PSTR("Bad Request"), PSTR("Not enough files were provided"));
			else send_static_response(204, PSTR("No Content"), PSTR(""));
		}
		else if (strcasecmp_P(tgt, PSTR("newui")) == 0) {
			if (reqstate->c.method != HTTP_SERVE_METHOD_POST) goto invmethod;

			if (!sd.exists("/upd")) sd.mkdir("/upd");

			File out_ui = sd.open("/upd/webui.ar", O_CREAT | O_WRITE | O_TRUNC);
			bool ok = false;
			switch (do_multipart([&](uint8_t * buf, int len, multipart_header_state_t *state){
				if (!state->name_counter) {
					Log.println(F("no name"));
					return false;
				}
				
				if (len == -1) {
					return true;
				}
				else if (len == -2) {
					out_ui.flush();
					out_ui.close();
					ok = true;
					return true;
				}
				else {
					out_ui.write(buf, len);
					return true;
				}
			})) {
				case MultipartStatus::EOF_EARLY:
					return;
				case MultipartStatus::INVALID_HEADER:
					send_static_response(400, PSTR("Bad Request"), PSTR("The server was unable to interpret the header area of the form data request."));
					return;
				case MultipartStatus::HOOK_ABORT:
					send_static_response(400, PSTR("Bad Request"), PSTR("You have sent invalid files."));
					return;
				case MultipartStatus::NO_CONTENT_DISP:
					send_static_response(400, PSTR("Bad Request"), PSTR("You are missing a Content-Disposition header in your request."));
					return;
				case MultipartStatus::OK:
					break;
				default:
					return;
			}

			if (!ok) {
				send_static_response(400, PSTR("Bad Request"), PSTR("Not enough files were provided"));
				return;
			}
			
			out_ui = sd.open("/upd/state.txt", O_WRITE | O_CREAT | O_TRUNC);
			out_ui.print(16);
			out_ui.flush();
			out_ui.close();

			send_static_response(200, PSTR("OK"), PSTR("Updating UI."));
			delay(100);
			ESP.restart();
		}
		else if (strcasecmp_P(tgt, PSTR("updatefirm")) == 0) {
			if (reqstate->c.method != HTTP_SERVE_METHOD_POST) goto invmethod;

			File out_file;
			int gotcount = 0;

			uint16_t stm_csum = 0, esp_csum = 0;

			// Make the update directory
			if (!sd.exists("/upd"))
				sd.mkdir("/upd");

			// Temp.
			switch (do_multipart([&](uint8_t * buf, int len, multipart_header_state_t *state){
				if (!state->name_counter) {
					Log.println(F("no name"));
					return false;
				}

				if (len == -1) {
					if (strcasecmp_P(state->c.name, PSTR("stm")) == 0) {
						out_file = sd.open("/upd/stm.bin", O_WRITE | O_CREAT | O_TRUNC);
						Log.println(F("Writing the stm bin"));
						stm_csum = 0;
						return true;
					}
					if (strcasecmp_P(state->c.name, PSTR("esp")) == 0) {
						out_file = sd.open("/upd/esp.bin", O_WRITE | O_CREAT | O_TRUNC);
						Log.println(F("Writing the esp bin"));
						esp_csum = 0;
						return true;
					}
					else {
						Log.println(F("Ignoring"));
						return false;
					}
				}
				else if (len == -2) {
					out_file.flush();
					out_file.close();
					++gotcount;
				}
				else {
					// Write the buffer
					out_file.write(buf, len);

					// Update the checksum
					if (strcasecmp_P(state->c.name, PSTR("stm")) == 0) 
						stm_csum = util::compute_crc(buf, len, stm_csum);
					else 												
						esp_csum = util::compute_crc(buf, len, esp_csum);
				}
				return true;
			})) {
				case MultipartStatus::EOF_EARLY:
					return;
				case MultipartStatus::INVALID_HEADER:
					send_static_response(400, PSTR("Bad Request"), PSTR("The server was unable to interpret the header area of the form data request."));
					return;
				case MultipartStatus::HOOK_ABORT:
					send_static_response(400, PSTR("Bad Request"), PSTR("You have sent invalid files."));
					return;
				case MultipartStatus::NO_CONTENT_DISP:
					send_static_response(400, PSTR("Bad Request"), PSTR("You are missing a Content-Disposition header in your request."));
					return;
				case MultipartStatus::OK:
					break;
				default:
					return;
			}

			if (gotcount < 2) {
				send_static_response(400, PSTR("Bad Request"), PSTR("Not enough files were provided"));
				return;
			}

			// Alright we've got stuff. Write the csum file now.
			out_file = sd.open("/upd/chck.sum", O_WRITE | O_CREAT | O_TRUNC);
			out_file.print(esp_csum);
			out_file.print(' ');
			out_file.print(stm_csum);
			out_file.flush();
			out_file.close();

			// Set the update state for update
			out_file = sd.open("/upd/state.txt", O_WRITE | O_CREAT | O_TRUNC);
			out_file.print(0); // Update system (state 0 in upd.cpp)
			out_file.flush();
			out_file.close();

			send_static_response(200, PSTR("OK"), PSTR("Starting the update."));
			delay(100); // give it some time to send it
			serial::interface.reset();
		}
		else if (strcasecmp_P(tgt, PSTR("reboot")) == 0) {
			// Just reboot
			send_static_response(204, PSTR("No Content"), PSTR(""));
			serial::interface.reset();
		}
		else if (strcasecmp_P(tgt, PSTR("updatestm")) == 0) {
			if (reqstate->c.method != HTTP_SERVE_METHOD_POST) goto invmethod;

			File out_file = sd.open("/upd/stm.bin", O_WRITE | O_CREAT | O_TRUNC);
			bool ok = false;

			uint16_t stm_csum = 0;

			// Make the update directory
			if (!sd.exists("/upd"))
				sd.mkdir("/upd");

			// Temp.
			switch (do_multipart([&](uint8_t * buf, int len, multipart_header_state_t *state){
				if (!state->name_counter) {
					Log.println(F("no name"));
					return false;
				}

				if (len == -1) {
					return strcasecmp_P(state->c.name, PSTR("stm")) == 0;
				}
				else if (len == -2) {
					out_file.flush();
					out_file.close();
					ok = true;
				}
				else {
					// Write the buffer
					out_file.write(buf, len);

					// Update the checksum
					stm_csum = util::compute_crc(buf, len, stm_csum);
				}
				return true;
			})) {
				case MultipartStatus::EOF_EARLY:
					return;
				case MultipartStatus::INVALID_HEADER:
					send_static_response(400, PSTR("Bad Request"), PSTR("The server was unable to interpret the header area of the form data request."));
					return;
				case MultipartStatus::HOOK_ABORT:
					send_static_response(400, PSTR("Bad Request"), PSTR("You have sent invalid files."));
					return;
				case MultipartStatus::NO_CONTENT_DISP:
					send_static_response(400, PSTR("Bad Request"), PSTR("You are missing a Content-Disposition header in your request."));
					return;
				case MultipartStatus::OK:
					break;
				default:
					return;
			}

			if (!ok) {
				send_static_response(400, PSTR("Bad Request"), PSTR("The stm firmware was not provided"));
				return;
			}

			// Alright we've got stuff. Write the csum file now.
			out_file = sd.open("/upd/chck.sum", O_WRITE | O_CREAT | O_TRUNC);
			out_file.print(0);
			out_file.print(' ');
			out_file.print(stm_csum);
			out_file.flush();
			out_file.close();

			// Ensure there isn't an errant /upd/esp.bin
			if (sd.exists("/upd/esp.bin")) sd.remove("/upd/esp.bin");

			// Set the update state for update
			out_file = sd.open("/upd/state.txt", O_WRITE | O_CREAT | O_TRUNC);
			out_file.print(0); // Update system (state 0 in upd.cpp)
			out_file.flush();
			out_file.close();

			send_static_response(200, PSTR("OK"), PSTR("Starting the update."));
			delay(100); // give it some time to send it
			serial::interface.reset();
		}
		else {
			// Temp.
			send_static_response(404, PSTR("Not Found"), PSTR("Api method not recognized."));
		}
		return;
invmethod:
		send_static_response(405, PSTR("Method Not Allowed"), PSTR("Invalid API method."));
		return;
badrequest:
		send_static_response(400, PSTR("Bad Request"), PSTR("Invalid parameters to API method."));
		return;
notfound:
		send_static_response(404, PSTR("Not Found"), PSTR("The data that resource points to does not exist."));
		return;
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

		Log.println(f.size());

		if (reqstate->c.is_gzipable) activeClient.printf_P(PSTR("Content-Encoding: gzip\r\n"));
		stream_file(f);
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
				if (!check_auth(reqstate->c.auth_string)) {
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
			// Only allow GET requests
			if (reqstate->c.method != HTTP_SERVE_METHOD_GET) {
				send_static_response(405, PSTR("Method Not Allowed"), PSTR("Static resources are only gettable with GET"));
				return;
			}
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
				content_type = PSTR("text/html; charset=UTF-8");
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
		}
		else {
			activeClient = webserver.available();
			if (activeClient) {
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
