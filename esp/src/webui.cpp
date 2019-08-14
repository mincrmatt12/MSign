#include "webui.h"
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <SdFat.h>
#include "config.h"

// routines for loading the sd card content
extern SdFatSoftSpi<D6, D2, D5> sd;

// config defaults
#define DEFAULT_USERNAME "admin"
#define DEFAULT_PASSWORD "admin"

namespace webui {

	WiFiServer webserver(80);
	WiFiClient webclient;

	bool auth = false;

	enum WebState {
		WAITING_FOR_REQUEST = 0x00,
		WAITING_FOR_PATH,
		WAITING_FOR_VERSION,

		// Now the path splits, in some cases we'll be waiting for headers and content-type stuff, but most of the time we just check auth and send a file. This is flow 0x10.
		WAITING_FOR_AUTH_HEADER = 0x10,
		WAITING_FOR_AUTH_HEADER2, // grabbing token
		WAITING_FOR_AUTH_HEADER3, // skipping to next header
		WAITING_FOR_END_OF_HEADERS,
		WAITING_FOR_END_OF_HEADERS2,
		SENDING_FILE_CONTENT, // sending the file content after initial headers sent

		// For API GET calls, we just need to parse the AUTH header and send it to the right function. In this case the state goes back to flow 0x10, but calls the _getapi func at the end and ends the session.
		// For API POST calls, we need to do a bit more -- i.e. parsing the message body as well.

		FILLING_UP_REQUEST_HEADER_DATA = 0x20,  // checking header name
		READING_REQUEST_BODY, // reading the request body

		// Afterwards, we call the _postapi func.

		// Soon, there will be things here to read stuff like form data for files.

		// extra states:
		SEND_INVALID_AUTH = 0x30,  // send a 401
		SEND_INVALID_REQUEST,	   // send a 400
		SEND_NOTFOUND,   		   // send a 404
		SEND_NORMAL_FILE,          // send a file (call _endrequest())
		SEND_CONFIG_TXT,		   // send the config (serialized from memory)

		// post states (link_State interpreted as READ_REQUEST_BODY in certian cases)
		SEND_CONFIG_WRITE = 0x50,

		// api send states

		NOT_CONNECTED = 0xff
	} web_state = NOT_CONNECTED, link_state;



	File sending_file;
	char name_buffer[64];
	int name_idx = 0;
	char method_buffer[8];
	int method_idx = 0;
	char header_buffer[64];
	int header_idx;
	char * body_buffer = nullptr;
	int body_idx;
	int body_size = 0;

	void init() {
		sd.chdir("/web");
		if (!sd.exists("page.html") || !sd.exists("page.js") || !sd.exists("page.css")) {
			Serial1.println(F("SD doesn't contain the web/ folder contents"));
			delay(1000);
			ESP.restart();
		}
		sd.chdir();

		// Initialize the webserver

		webserver.begin();
	}

	bool _authvalid(const char * in) {
		// Encode the current parameter.
		static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
										'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
										'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
										'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
										'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
										'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
										'w', 'x', 'y', 'z', '0', '1', '2', '3',
										'4', '5', '6', '7', '8', '9', '+', '/'};
		static int mod_table[] = {0, 2, 1};

		auto username = config::manager.get_value(config::CONFIG_USER, DEFAULT_USERNAME);
		auto password = config::manager.get_value(config::CONFIG_PASS, DEFAULT_PASSWORD);
		size_t orig_len = strlen(username) + strlen(password) + 1;
		size_t enc_len = 4 * ((orig_len + 2) / 3);
		if (enc_len % 3 != 0) {
			enc_len += (3 - enc_len % 3);
		}

		// get the enc length
		if (strlen(in) != enc_len) return false;

		char buf_orig[orig_len];
		strcpy(buf_orig, username);
		buf_orig[strlen(username)] = ':';
		strcpy(buf_orig + 1 + strlen(username), password);
		char buf[enc_len];

		for (int i = 0, j = 0; i < orig_len;) {
			uint32_t octet_a = i < orig_len ? (unsigned char)buf_orig[i++] : 0;
			uint32_t octet_b = i < orig_len ? (unsigned char)buf_orig[i++] : 0;
			uint32_t octet_c = i < orig_len ? (unsigned char)buf_orig[i++] : 0;

			uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

			buf[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
			buf[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
			buf[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
			buf[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
		}

		for (int i = 0; i < mod_table[orig_len % 3]; i++)
			buf[enc_len - 1 - i] = '=';
		
		return strncmp(buf, in, enc_len) == 0;
	}

	// send data from a file
	
	void _send_resp_header(const char * mime_type) {
		webclient.println(F("HTTP/1.1 200 OK"));
		webclient.print(F("Content-Type: ")); webclient.println(mime_type);
		webclient.print(F("Content-Length: ")); webclient.println(sending_file.size());
		webclient.println(F("Connection: close"));
		webclient.println();
	}

	bool _send_resp_file() {
		if (!sending_file.isOpen()) return true;  // done

		size_t j = 128;
		if (sending_file.available() < 128) {
			j = sending_file.available();
		}

		if (j == 0) {
			sending_file.close();
			return true;
		}

		uint8_t buf[j];
		sending_file.read(buf, j);
		webclient.write(buf, j);

		return false;
	}

	void _reinit() {
		method_idx = 0;
		name_idx = 0;
		header_idx = 0;
		auth = false;

		free(body_buffer);
		body_idx = 0;
		body_size = 0;

		if (sending_file.isOpen())
			sending_file.close();
	}

	void _apiget(const char * path, size_t size) {
		// check what the user wants
		if (size == 8 && strncmp(path, "conf.txt", size) == 0) {
			// set the link state to _send conf_
			
			web_state = WAITING_FOR_AUTH_HEADER; // only check for auth headers
			link_state = SEND_CONFIG_TXT;
			return;
		}

		web_state = SEND_NOTFOUND;
	}
	void _apipost(const char * path, size_t size) {
		// currently does nothing, but soon will do _much more_ :tm:
		if (size == 8 && strncmp(path, "conf.txt", size) == 0) {
			// set the link state to _send conf_
			
			web_state = FILLING_UP_REQUEST_HEADER_DATA; // only check for auth headers
			link_state = SEND_CONFIG_WRITE;
			return;
		}

		web_state = SEND_INVALID_REQUEST;
	}

	void _startrequest() {
		if (name_idx < 1 || name_buffer[0] != '/') {
			web_state = SEND_NOTFOUND;
			return;
		}

		if (strncmp(name_buffer, "/favicon.ico", name_idx) == 0) {
			web_state = SEND_NOTFOUND;
			return;
		}

		if (name_idx > 3 && name_buffer[1] == 'a') {
			if (strncmp(method_buffer, "GET", method_idx) == 0) _apiget(&name_buffer[3], name_idx - 3);
			else if (strncmp(method_buffer, "POST", method_idx) == 0) _apipost(&name_buffer[3], name_idx - 3);
			else {
				web_state = SEND_INVALID_REQUEST;
			}
			return;
		}

		// Begin checking for auth headers
		web_state = WAITING_FOR_AUTH_HEADER;
		link_state = SEND_NORMAL_FILE;
	}

	void _endrequest() {
		// this is only called for normal file requests (sometimes 
		
		const char * fname;
		const char * mt;
		if (strncmp(name_buffer, "/page.css", name_idx) == 0) {
			fname = "page.css";
			mt = "text/css";
		}
		else if (strncmp(name_buffer, "/page.js", name_idx)  == 0) {
			fname = "page.js";
			mt = "application/javascript";
		}
		else {
			fname = "page.html";
			mt = "text/html";
		}

		sd.chdir("web", true);
		sending_file = sd.open(fname);
		sd.chdir(true);

		_send_resp_header(mt);

		web_state = SENDING_FILE_CONTENT;
	}

	void loop() {
		if (web_state == NOT_CONNECTED) {
			webclient = webserver.available();
			if (webclient) {
				web_state = WAITING_FOR_REQUEST;

				_reinit();
			}
			return;
		}

		switch (web_state) {
			case WAITING_FOR_REQUEST:
				{
					while (webclient.available()) {
						if (method_idx == 8) {
							// Invalid request.
							
							web_state = SEND_INVALID_REQUEST;
							return;
						}
						char c = webclient.read();
						if (c == ' ') {
							web_state = WAITING_FOR_PATH;
							return;
						}
						else {
							method_buffer[method_idx++] = c;
						}
					}
				}
				return;
			case SEND_INVALID_REQUEST:
				{
					webclient.println(F("HTTP/1.1 400 Bad Request"));
					webclient.println(F("Content-Type: text/plain"));
					webclient.println(F("Connection: close"));
					webclient.println();
					webclient.println(F("bad request"));
					yield();
					webclient.stop();
					web_state = NOT_CONNECTED;
				}
				return;
			case SEND_NOTFOUND:
				{
					webclient.println(F("HTTP/1.1 404 Not Found"));
					webclient.println(F("Content-Type: text/plain"));
					webclient.println(F("Connection: close"));
					webclient.println();
					webclient.println(F("not found"));
					yield();
					webclient.stop();
					web_state = NOT_CONNECTED;
				}
				return;
			case WAITING_FOR_PATH:
				{
					while (webclient.available()) {
						if (name_idx == 64) {
							// Invalid request.
							
							web_state = SEND_INVALID_REQUEST;
							return;
						}
						char c = webclient.read();
						if (c == ' ') {
							web_state = WAITING_FOR_VERSION;
							return;
						}
						else {
							name_buffer[name_idx++] = c;
						}
					}
				}
				return;
			case WAITING_FOR_VERSION:
				{
					if (webclient.available() > 8) {
						char buffer[9];
						webclient.readBytes(buffer, 9);

						if (strncmp(buffer, "HTTP/1.1", 8) == 0 || strncmp(buffer, "HTTP/1.0", 8) == 0) {
							yield();
							if (buffer[8] == '\r') webclient.read();
							
							_startrequest();
						}
						else {
							web_state = SEND_INVALID_REQUEST;
						}
					}
				}
				return;
			case WAITING_FOR_AUTH_HEADER:
				{
					while (webclient.available()) {
						if (header_idx == 64) {
							// Invalid request.
							
							web_state = SEND_INVALID_REQUEST;
							return;
						}
						char c = webclient.read();
						if (c == ' ') {
							if (strncmp(header_buffer, "Authorization:", header_idx) == 0)
								{ web_state = WAITING_FOR_AUTH_HEADER2; header_idx = 0; }
							else 
								web_state = WAITING_FOR_AUTH_HEADER3;
						}
						else if (c == '\r' || c == '\n') {
							yield();
							if (c == '\r') {
								webclient.read();
							}

							web_state = SEND_INVALID_AUTH;
						}
						else {
							header_buffer[header_idx++] = c;
						}
					}
				}
				return;
			case WAITING_FOR_AUTH_HEADER3:
				{
					while (webclient.available()) {
						if (webclient.read() == '\n') {
							web_state = link_state < SEND_CONFIG_WRITE ? WAITING_FOR_END_OF_HEADERS : FILLING_UP_REQUEST_HEADER_DATA;
							header_idx = 0;
						}
					}
				}
				return;
			case WAITING_FOR_AUTH_HEADER2:
				{
					while (webclient.available()) {
						if (header_idx == 64) {
							// Invalid request.
							
							web_state = SEND_INVALID_REQUEST;
							return;
						}
						char c = webclient.read();
						if (c == ' ') {
							if (strncmp(header_buffer, "Basic", header_idx) != 0) {
								web_state = SEND_INVALID_AUTH;
								return;
							}
						}
						else if (c == '\r' || c == '\n') {
							char obuf[header_idx - 4];
							obuf[header_idx - 5] = 0;

							memcpy(obuf, header_buffer + 5, header_idx - 5);
							if (!_authvalid(obuf)) {
								// invalid auth
								web_state = SEND_INVALID_AUTH;
								return;
							}
							else {
								yield();
								if (c == '\r') webclient.read();
								auth = true;
								web_state = link_state < SEND_CONFIG_WRITE ? WAITING_FOR_END_OF_HEADERS : FILLING_UP_REQUEST_HEADER_DATA;
								return;
							}
						}
						else {
							header_buffer[header_idx++] = c;
						}
					}
				}
				return;
			case FILLING_UP_REQUEST_HEADER_DATA:
				{
					if (body_size > 0 && auth) {
						auth = false;
						web_state = WAITING_FOR_END_OF_HEADERS;
					}
					while (webclient.available()) {
						if (header_idx == 64) {
							// Invalid request.
							
							web_state = SEND_INVALID_REQUEST;
							return;
						}
						char c = webclient.read();
						if (c == ' ') {
							if (strncmp(header_buffer, "Authorization:", header_idx) == 0)
								{ web_state = WAITING_FOR_AUTH_HEADER2; header_idx = 0; }
							else if (strncmp(header_buffer, "Content-Length:", header_idx) == 0) 
								{ header_idx = 0; body_size = webclient.parseInt(); web_state = WAITING_FOR_AUTH_HEADER3;}
							else 
								web_state = WAITING_FOR_AUTH_HEADER3; // reused here due to how link_state works
						}
						else if (c == '\r' || c == '\n') {
							yield();
							if (c == '\r') {
								webclient.read();
							}

							web_state = SEND_INVALID_REQUEST;
						}
						else {
							header_buffer[header_idx++] = c;
						}
					}
				}
				return;
			case WAITING_FOR_END_OF_HEADERS:
				{
					if (webclient.available()) {
						char c = webclient.read();
						if (c == '\r' || c == '\n') {
							if (c == '\r') {
								yield();
								webclient.read();
							}

							web_state = link_state;
							return;
						}
						else {
							web_state = WAITING_FOR_END_OF_HEADERS2;
						}
					}
				}
				return;
			case READING_REQUEST_BODY:
				{
					if (body_buffer == nullptr) {
						body_buffer = (char *) malloc(body_size);
					}

					if (webclient.available()) {
						if (body_idx >= body_size) {
							while (webclient.available()) {
								yield();
								webclient.read();
							}
							web_state = link_state;
							break;
						}
						int tocopy = min(body_size - body_idx, webclient.available());
						webclient.readBytes(body_buffer + body_idx, tocopy);
						body_idx += tocopy;
					}
				}
				return;
			case WAITING_FOR_END_OF_HEADERS2:
				{
					while (webclient.available()) {
						char c = webclient.read();
						if (c == '\r' || c == '\n') {
							if (c == '\r') {
								yield();
								webclient.read();
							}
							web_state = WAITING_FOR_END_OF_HEADERS;
						}
					}
				}
				return;
			case SEND_NORMAL_FILE:
				{
					_endrequest();
				}
				return;
			case SEND_INVALID_AUTH:
				{
					webclient.println(F("HTTP/1.1 401 Not Authorized"));
					webclient.println(F("Content-Type: text/plain"));
					webclient.println(F("WWW-Authenticate: Basic realm=\"msign\""));
					webclient.println(F("Connection: close"));
					webclient.println();
					webclient.println(F("enter password plz / wrong password"));
					yield();
					webclient.stop();
					web_state = NOT_CONNECTED;
				}
				return;
			case SENDING_FILE_CONTENT:
				{
					if (_send_resp_file()) {
						yield();
						webclient.stop();
						web_state = NOT_CONNECTED;
					}
				}
				return;
			case SEND_CONFIG_TXT:
				{
					webclient.println(F("HTTP/1.1 200 OK"));
					webclient.println(F("Content-Type: text/plain"));
					webclient.println(F("Connection: close"));
					webclient.println();
					for (uint8_t e = 0; e < config::ENTRY_COUNT; ++e) {
						const char * value;
						if ((value = config::manager.get_value((config::Entry)e)) != nullptr) {
							webclient.print(config::entry_names[e]);
							webclient.print('=');
							webclient.print(value);
							webclient.print('\n');
						}
					}
					yield();
					webclient.stop();
					web_state = NOT_CONNECTED;
				}
				return;
			case SEND_CONFIG_WRITE:
				{
					// try to send the new txt
					if (config::manager.use_new_config(body_buffer, body_size)) {
						webclient.println(F("HTTP/1.1 204 No Content But It Still Worked"));
						webclient.println(F("Connection: close"));
						webclient.println();
						yield();
						webclient.stop();
					}
					else {
						webclient.println(F("HTTP/1.1 500 Server Error"));
						webclient.println(F("Content-Type: text/plain"));
						webclient.println(F("Connection: close"));
						webclient.println();
						webclient.println(F("couldn't write sd"));
						yield();
						webclient.stop();
					}
					web_state = NOT_CONNECTED;
					free(body_buffer);
				}
			default:
				return;
		}
	}
}
