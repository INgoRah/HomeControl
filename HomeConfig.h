#ifndef HOMECONFIG_H
#define HOMECONFIG_H

#include <Arduino.h>

class HomeConfig {
	private:
		void WriteStringToEEPROM(int adr, String s, size_t size);
		String ReadStringFromEEPROM(int beginaddress);
	protected:
	public:
		HomeConfig() {
			ssid = "";
			password = "";
		}
		int init ();
		void readConfig();
		void writeConfig();
		bool telegram;
		bool presence;
	char* ssid;
	char* password;
};

#endif
