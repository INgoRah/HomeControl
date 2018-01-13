
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
	String s;

	EEPROM.begin(512);
	if (EEPROM.read(pos++) == 'C' && EEPROM.read(pos++) == 'F'  && EEPROM.read(pos++) == 'G') {
		Serial.println ();
		Serial.println (F("Reading config"));
		Serial.print("  SID=");
		s = ReadStringFromEEPROM(pos);
		Serial.println (s);
		pos += 32;
		Serial.print("  passwd=");
		s = ReadStringFromEEPROM(pos);
		//password = s.c_str();
		Serial.println(s);
		pos += 32;
		presence = EEPROM.read(pos++);
		Serial.printf("  presense=%d\n", presence);
		telegram = EEPROM.read(pos++);
		Serial.printf("  telegram=%d\n", telegram);
	}
}

void HomeConfig::writeConfig()
{
	uint32_t pos = 0;

	Serial.println(F("Writing Config"));
	EEPROM.write(pos++, 'C');
	EEPROM.write(pos++, 'F');
	EEPROM.write(pos++, 'G');

	WriteStringToEEPROM(pos, ssid, 31);
	pos += 32;
	WriteStringToEEPROM(pos, password, 31);
	pos += 32;
	EEPROM.write(pos++, presence);
	EEPROM.write(pos++, telegram);
	EEPROM.commit();
}

