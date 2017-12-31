#ifndef HOMEWEB_H
#define HOMEWEB_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

class HomeConfig;

class HomeWeb {
	private:
		void _sendHeader(const bool form, const bool active, int size);
		void _sendFooter(const bool form, const bool active);
	protected:
		ESP8266WebServer * _server;
	public:
		HomeWeb() { ; }
		void setup(ESP8266WebServer *server);

		void startSection(String& content, const char* title);
		void endSection(String& content);
		void addButton (String& content, const char* label,
			int val, const char* url);
		void sendPage_P(const String &name, PGM_P content,
			bool form = true, bool active = true);
		void sendPage(const String &name, const String& content,
			bool form = true, bool active = true);
		void handleConfig(HomeConfig &cfg);
		void sendSensorsPage(String& content);
};

#endif
