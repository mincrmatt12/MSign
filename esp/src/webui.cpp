#include "webui.h"
#include <ESP8266WebServer.h>
#include <SdFat.h>
#include "config.h"
#include "serial.h"

// routines for loading the sd card content
extern SdFatSoftSpi<D6, D2, D5> sd;

// config defaults
#define DEFAULT_USERNAME "admin"
#define DEFAULT_PASSWORD "admin"

// webserver
ESP8266WebServer webserver;
char * etags[3];

namespace webui {
	// god i hate this thing's api, it really is poop
	
	bool _doauth(bool tryagain=true) {
		const char * user = config::manager.get_value(config::CONFIG_USER, DEFAULT_USERNAME);
		const char * pass = config::manager.get_value(config::CONFIG_PASS, DEFAULT_PASSWORD);

		if (!webserver.authenticate(user, pass)) {
			if (tryagain) {
				webserver.requestAuthentication(DIGEST_AUTH, "msign");
			}
			return false;
		}
		return true;
	}

	void _stream(const char * f, const char * mt, const char * etag) {
		if (webserver.hasHeader(FPSTR("If-None-Match"))) {
			if (webserver.header(FPSTR("If-None-Match")).indexOf(etag) >= 0) {
				webserver.sendHeader(FPSTR("Cache-Control"), FPSTR("max-age=0, must-revalidate"));
				webserver.sendHeader(FPSTR("Etag"), etag);
				webserver.send(304);
			}
		}

		sd.chdir("/web");
		File fl = sd.open(f, FILE_READ);
		if (!fl.isOpen()) {
			sd.chdir();
			webserver.send(404, "text/plain", FPSTR("file missing on sd"));
		}
		else {
			sd.chdir();

			webserver.sendHeader(FPSTR("Cache-Control"), FPSTR("max-age=0, must-revalidate"));
			webserver.sendHeader(FPSTR("Etag"), etag);
			webserver.streamFile(fl, mt);
			fl.close();
		}
	}

	void _notfound() {
		if (!_doauth()) return;
		auto uri = webserver.uri();
		if (uri == "/favicon.ico") webserver.send(404, "text/plain", "no ico");
		else if (uri == "/page.js") _stream("page.js", "text/javascript;charset=utf-8", etags[0]);
		else if (uri == "/page.css") _stream("page.css", "text/css;charset=utf-8", etags[1]);
		else _stream("page.html", "text/html;charset=utf-8", etags[2]);
	}

	void init() {
		// generate a random etag
		{
			char etag_buffer[16] = {0};
			snprintf(etag_buffer, 16, "\"E%9ldjs\"", secureRandom(ESP.getCycleCount() + ESP.getChipId() + ESP.getVcc()));
			etags[0] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, "\"E%9ldcs\"", secureRandom(ESP.getCycleCount() + ESP.getChipId() + ESP.getVcc()));
			etags[1] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, "\"E%9ldht\"", secureRandom(ESP.getCycleCount() + ESP.getChipId() + ESP.getVcc()));
			etags[2] = strdup(etag_buffer);
		}

		{
			const char * x = "If-None-Match";
			webserver.collectHeaders(&x, 1);
		}
		webserver.onNotFound(_notfound);
		webserver.on("/a/conf.txt", HTTP_GET, [](){
				if (!_doauth()) return;

				// stream the config file
				File fl = sd.open("config.txt", FILE_READ);
				webserver.streamFile(fl, "text/plain");
				fl.close();
		});
		webserver.on("/a/conf.txt", HTTP_POST, [](){
				if (!_doauth()) return;

				// check if we got the body
				if (!webserver.hasArg(FPSTR("plain"))) {
					webserver.send(400, "text/plain", FPSTR("send the new config"));
					return;
				}

				{
					String s = webserver.arg("plain");
					config::manager.use_new_config(s.c_str(), s.length());
				}

				// the new config now exists
				webserver.send(204);
		});
		webserver.on("/a/reboot", HTTP_GET, [](){
				if (!_doauth()) return;

				webserver.send(204);
				serial::interface.reset();
		});

		webserver.begin();
	}
	void loop() {
		webserver.handleClient();
	}
}
