
#include "HomeConfig.h"
#include <EEPROM.h>

int HomeConfig::init() {
	return 0;
}

void HomeConfig::WriteStringToEEPROM(int adr, String s, size_t size)
{
	uint32_t i;

	for (i = 0; i < s.length() && i < size; i++) {
		EEPROM.write(adr + i, s[i]);
	}
	EEPROM.write(adr + i + 1, 0);
}

String HomeConfig::ReadStringFromEEPROM(int beginaddress)
{
	byte counter = 0;
	char rChar;
	String retString = "";

	while (1)
	{
		rChar = EEPROM.read(beginaddress + counter);
		if (rChar == 0 || rChar == 0xff)
			break;
		if (counter > 31) break;
		counter++;
		retString.concat(rChar);

	}
	return retString;
}

void HomeConfig::readConfig() {
	uint32_t pos = 0;

	EEPROM.begin(512);
	if (EEPROM.read(pos++) == 'C' && EEPROM.read(pos++) == 'F'  && EEPROM.read(pos++) == 'G') {
		Serial.print("sid=");
		Serial.println (ReadStringFromEEPROM(pos));
		pos += 31;
		Serial.print(" passwd=");
		Serial.println(ReadStringFromEEPROM(pos));
		pos += 31;
		presence = EEPROM.read(pos++);
		Serial.printf(" det=%d\n", presence);
		presence = 0;
		telegram = EEPROM.read(pos++);
		Serial.printf("telegram=%d\n", telegram);
	}
	telegram = 0;
}

void HomeConfig::writeConfig()
{
	uint32_t pos = 0;

	Serial.println("Writing Config");
	EEPROM.write(pos++, 'C');
	EEPROM.write(pos++, 'F');
	EEPROM.write(pos++, 'G');

	WriteStringToEEPROM(pos, ssid, 31);
	pos += 31;
	WriteStringToEEPROM(pos, password, 31);
	pos += 31;
	EEPROM.write(pos++, presence);
	EEPROM.write(pos++, telegram);
	EEPROM.commit();
}

