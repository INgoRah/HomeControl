#ifndef HOMEWEB_H
#define HOMEWEB_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

class HomeWeb {
	private:
		ESP8266WebServer* _server;
	protected:
	public:
		HomeWeb() { ; }
		int init (ESP8266WebServer* server);
		void startPage(String& content);
		void startPage(String& content, String data);
		void endPage(String& content, int ow_intPeding);
		void startSection(String& content, const char* title);
		void endSection(String& content);
		void addButton (String& content, const char* label,
			int val, const char* url);
		void handleConfig (int presence_det, int telegram, String& content);
};

#endif
