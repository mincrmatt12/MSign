#include "webui.h"
#include <ESP8266WebServer.h>
#include <SdFat.h>
#include "config.h"
#include "serial.h"
#include "common/util.h"
#include "util.h"

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
	
	// update params
	bool package_ok = false;
	int recieved_files = 0;
	File writing_update;
	uint16_t esp_crc = 0, stm_crc = 0;
	
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
		if (webserver.hasHeader(F("If-None-Match"))) {
			if (webserver.header(F("If-None-Match")).indexOf(etag) >= 0) {
				webserver.sendHeader(F("Cache-Control"), F("max-age=2630000, stale-while-revalidate=10520000"));
				webserver.sendHeader(F("Etag"), etag);
				webserver.send(304);
				return;
			}
		}

		sd.chdir("/web");
		File fl = sd.open(f, FILE_READ);
		if (!fl.isOpen()) {
			sd.chdir();
			webserver.send(404, "text/plain", F("file missing on sd"));
		}
		else {
			sd.chdir();

			webserver.sendHeader(F("Cache-Control"), F("max-age=0, must-revalidate"));
			webserver.sendHeader(F("Etag"), etag);
			webserver.streamFile(fl, mt);
			fl.close();
		}
	}

	void _notfound() {
		if (!_doauth()) return;
		auto uri = webserver.uri();
		if (uri.length() > 2 && uri[0] == '/' && uri[1] == 'a' && uri[2] == '/') {
			webserver.send(404, "text/plain", "no such api method");
		}
		if (uri == "/favicon.ico") webserver.send(404, "text/plain", "no ico");
		else if (uri == "/page.js") _stream("page.js", "text/javascript;charset=utf-8", etags[0]);
		else if (uri == "/page.css") _stream("page.css", "text/css;charset=utf-8", etags[1]);
		else _stream("page.html", "text/html;charset=utf-8", etags[2]);
	}

	void init() {
		// check if an etag file exists on disk (deleted during updates)
		if (!sd.exists("/web/etag.txt")) {
			char etag_buffer[16] = {0};
			auto etag_num = secureRandom(ESP.getCycleCount() + ESP.getChipId() + ESP.getVcc());
			snprintf(etag_buffer, 16, "\"E%9ldjs\"", etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, "\"E%9ldcs\"", etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, "\"E%9ldht\"", etag_num);
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

			snprintf(etag_buffer, 16, "\"E%9ldjs\"", etag_num);
			etags[0] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, "\"E%9ldcs\"", etag_num);
			etags[1] = strdup(etag_buffer);
			snprintf(etag_buffer, 16, "\"E%9ldht\"", etag_num);
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
				if (!webserver.hasArg(F("plain"))) {
					webserver.send(400, "text/plain", F("send the new config"));
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
		webserver.on("/a/newui", HTTP_POST, [](){
				if (!_doauth()) {
					// make sure we delete the update files.
					if (sd.chdir("/upd")) {
						sd.remove("webui.ar");
						sd.chdir();
					}
					return;
				}
				if (!package_ok) {
					webserver.send(500, "text/plain", "invalid upload/aborted/something");
					return;
				}

				sd.chdir("/upd");
				File f = sd.open("state.txt", FILE_WRITE);
				f.print("16"); // update UI
				f.flush();
				f.close();

				sd.chdir();
				webserver.send(200, "text/plain", "updating now");
				delay(100);
				ESP.restart();
		}, [](){
			// file upload handler
			auto& upload = webserver.upload();

			switch (upload.status) {
				case UPLOAD_FILE_START: 
					{
						if (!sd.exists("/upd")) sd.mkdir("/upd");
						sd.chdir("/upd");
						writing_update = sd.open("webui.ar", FILE_WRITE);
						sd.chdir();

						Log.println(F("writing data from update to webui.ar"));
					}
					break;
				case UPLOAD_FILE_END:
					{
						package_ok = true;
						writing_update.flush();
						writing_update.close();
					}
					break;
				case UPLOAD_FILE_ABORTED:
					{
						writing_update.remove();
						writing_update.close();
						package_ok = false;
					}
					break;
				case UPLOAD_FILE_WRITE:
					{
						writing_update.write(upload.buf, upload.currentSize);
					}
					break;
			}
		});
		webserver.on("/a/updatestm", HTTP_POST, [](){
			if (!_doauth()) {
				// make sure we delete the update files.
				if (sd.chdir("/upd")) {
					sd.remove("stm.bin");
					sd.remove("esp.bin");
					sd.remove("chck.sum");
					sd.chdir();
				}
				return;
			}

			// verify we have the files
			if (recieved_files != 1) {
				webserver.send(400, F("text/plain"), F("invalid data"));
				recieved_files = 0;
				return;
			}

			// write out the checksum data
			File f = sd.open("/upd/chck.sum", FILE_WRITE);
			f.print(0);
			f.print(' ');
			f.print(stm_crc);
			f.flush();
			f.close();

			f = sd.open("/upd/state.txt", FILE_WRITE);
			f.print(0); // update SYS
			f.flush();
			f.close();

			if (sd.exists("/upd/esp.bin")) sd.remove("/upd/esp.bin");

			webserver.send(200, "text/plain", "ok i'm going jeez");
			delay(100);
			serial::interface.reset();
			ESP.restart();
		}, [](){
			// file upload handler
			auto& upload = webserver.upload();

			switch (upload.status) {
				case UPLOAD_FILE_START: 
					{
						if (!sd.exists("/upd")) sd.mkdir("/upd");
						sd.chdir("/upd");
						writing_update = sd.open("stm.bin", O_CREAT | O_WRITE | O_TRUNC);
						sd.chdir();

						Log.println(F("writing update upd1"));
						stm_crc = 0;
					}
					break;
				case UPLOAD_FILE_END:
					{
						writing_update.flush();
						writing_update.close();
						recieved_files = 1;
					}
					break;
				case UPLOAD_FILE_ABORTED:
					{
						writing_update.remove();
						writing_update.close();
					}
					break;
				case UPLOAD_FILE_WRITE:
					{
						writing_update.write(upload.buf, upload.currentSize);
						stm_crc = util::compute_crc(upload.buf, upload.currentSize, stm_crc);
					}
					break;
			}
		});
		webserver.on("/a/updatefirm", HTTP_POST, [](){
			if (!_doauth()) {
				// make sure we delete the update files.
				if (sd.chdir("/upd")) {
					sd.remove("stm.bin");
					sd.remove("esp.bin");
					sd.remove("chck.sum");
					sd.chdir();
				}
				return;
			}

			// verify we have the files
			if (recieved_files != 2) {
				webserver.send(400, F("text/plain"), F("invalid data"));
				recieved_files = 0;
				return;
			}

			// write out the checksum data
			File f = sd.open("/upd/chck.sum", FILE_WRITE);
			f.print(esp_crc);
			f.print(' ');
			f.print(stm_crc);
			f.flush();
			f.close();

			f = sd.open("/upd/state.txt", FILE_WRITE);
			f.print(0); // update UI
			f.flush();
			f.close();

			webserver.send(200, "text/plain", "ok i'm going jeez");
			delay(100);
			serial::interface.reset();
			// huh?
			ESP.restart();
			// HUH?
			Log.println(F("aaaaaaaaaaaa"));
		}, [](){
			// file upload handler
			auto& upload = webserver.upload();
			bool is_stm = upload.name.equals("stm");

			switch (upload.status) {
				case UPLOAD_FILE_START: 
					{
						if (!sd.exists("/upd")) sd.mkdir("/upd");
						sd.chdir("/upd");
						writing_update = sd.open(is_stm ? "stm.bin" : "esp.bin", O_CREAT | O_WRITE | O_TRUNC);
						sd.chdir();

						Log.println(F("writing update data"));
						(is_stm ? stm_crc : esp_crc) = 0;
					}
					break;
				case UPLOAD_FILE_END:
					{
						writing_update.flush();
						writing_update.close();
						++recieved_files;
					}
					break;
				case UPLOAD_FILE_ABORTED:
					{
						writing_update.remove();
						writing_update.close();
					}
					break;
				case UPLOAD_FILE_WRITE:
					{
						writing_update.write(upload.buf, upload.currentSize);
						(is_stm ? stm_crc : esp_crc) = util::compute_crc(upload.buf, upload.currentSize, (is_stm ? stm_crc : esp_crc));
					}
					break;
			}
		});
		webserver.on("/a/model.bin", HTTP_GET, [](){
				if (!_doauth()) return;
				if (!sd.exists("/model.bin")){
					webserver.send(404);
					return;
				}
				// stream the config file
				File fl = sd.open("model.bin", FILE_READ);
				webserver.streamFile(fl, "application/octet-stream");
				fl.close();
		});
		webserver.on("/a/model1.bin", HTTP_GET, [](){
				if (!_doauth()) return;
				if (!sd.exists("/model1.bin")){
					webserver.send(404);
					return;
				}
				// stream the config file
				File fl = sd.open("model1.bin", FILE_READ);
				webserver.streamFile(fl, "application/octet-stream");
				fl.close();
		});
		webserver.on("/a/mpres.json", HTTP_GET, [](){
				if (!_doauth()) return;
				bool m0 = sd.exists("/model.bin");
				bool m1 = sd.exists("/model1.bin");

				char buf[34] = {0};
				snprintf_P(buf, 34, PSTR("{\"m0\":%s,\"m1\":%s}"), m0 ? "true" : "false", m1 ? "true" : "false");

				webserver.send(200, "application/json", buf);
		});
		webserver.on("/a/newmodel", HTTP_POST, [](){
			// when the model is recieved literally do nothing.
			// no auth check because screw that crap
			
			webserver.send(204);
		}, [](){
			// upload data:
			auto& upload = webserver.upload();

			switch (upload.status) {
				case UPLOAD_FILE_START:
					{
						writing_update = sd.open((upload.name == "model") ? "/model.bin" : "/model1.bin", O_WRITE | O_CREAT | O_TRUNC);
					}
					break;
				case UPLOAD_FILE_END:
					{
						writing_update.flush();
						writing_update.close();
						++recieved_files;
					}
					break;
				case UPLOAD_FILE_ABORTED:
					{
						writing_update.remove();
						writing_update.close();
					}
					break;
				case UPLOAD_FILE_WRITE:
					{
						writing_update.write(upload.buf, upload.currentSize);
					}
					break;
			}
		});

		webserver.begin();
	}
	void loop() {
		webserver.handleClient();
	}
}
