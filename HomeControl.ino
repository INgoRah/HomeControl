/*
 * Global switchs
 */
#define USE_TR064
#define DEBUG
/*
 * Includes
 */
#include <time.h>
#include <sys/time.h> // struct timeval
#include <ESP8266WiFi.h>

/*
 * Library classs includes
 */
#include <ESP8266WebServer.h>
#ifndef WIN32
#include <ESP8266HTTPUpdateServer.h>
#endif
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <OneWireX.h>
#include <SysLogger.h>
#ifdef USE_TR064
#include "tr064.h"
#endif /* USE_TR064 */

/*
 * Local Includes
 */
#include "HomeControl.h"
#include "HomeWeb.h"
#include "HomeConfig.h"
#include "config.h"
#include "version.h"

#ifdef DEBUG
#define DBG(...) do { Log.printf( __VA_ARGS__ ); } while(0)
#else
#define DBG(...)
#endif

/*
 * Local constants
 */
/** standard LED, used on breadboard */
const char led = 2;

/** output for direct connected relais, low active */
#define PIN_LAMP_RELEAIS 10
/*  input of direct connected wall switch, active low */
#define PIN_LAMP_SW 9
/** OneWire interface pin */
#define PIN_OW 4
/** check interval for temperature e.g. */
#define STATUS_POLL_CYCLE (1000 * 120)
/** time to wait for new connection after switching off Wifi. Let FritzBox
  switch off its Wifi in inactive times */
#define DEADTIME (15 * 60 * 1000)
#define SLEEP_TO 8
/** time for polling interval on in/active devices */
#define ACT_POLL_TIME (1000 * 30)
#define MAX_SENSORS 5

#define IR_ONOFF 0x5EA1F807
#define IR_TUNER 0x5EA16897
#define IR_CD 0x5EA1A857
#define IR_VOLUP 0x5EA158A7
#define IR_VOLDN 0x5EA1D827

/*
 * Types
 */
enum WifiState {
	ST_INIT,
	/* Someone switched light, so don't switch off too fast */
	ST_ACT_KEEP,
	ST_ACT,
	// active but no users, so powering down soon
	ST_ACT_SLEEP,
	// we are in power down for some dead time to let 
	// FritzBox switch off Wifi
	ST_INACT_POWERDN,
	// forcing power down with a dead time (not checking for new connection)
	ST_INACT_FORCE,
	// inactive, but checking for Wifi connection
	ST_INACT_CHECK,
};

class OwDevice {
	private:
};

/*
* Function declarations
*/
void handleIdx();
void handleSensors();
void setState(WifiState s);
void powerDown();
bool connectWifi(char en, int wait);

/*
 * Objects
 */
ESP8266WebServer server(80);
#ifndef WIN32
ESP8266HTTPUpdateServer httpUpdater;
#endif
// on pin PIN_OW (a max 4.7K resistor is necessary)
OneWire ds(PIN_OW);
volatile boolean ow_intPending;
#ifdef USE_TR064
TR064 tr64(PORT, fritz_ip, fritz_user, fritz_passwd);
#endif /* USE_TR064 */
HomeWeb web;
HomeConfig cfg;
SysLogger Log;
#ifdef DYN_INDEX
static String main_page = String();
#endif
/*
 * Local variables
 */
time_t now;
static int lamp_st = 0;
static int lamp_sw = 0;
static unsigned long active_poll = 0, next_poll = 0;
static WifiState state;
static char bootTime[16];
static struct tm *currentTime;

static byte addr[MAX_SENSORS][8];
static byte sensors;
static int active_devs;
static int alarms;
static int sleepCountDown;

/* each switch contains two outputs (0=PowerLine, 1=LED), for temp it contains the 16 temp */
static union {
	byte temp[2];
	byte sw[4];
}sensor_data[MAX_SENSORS];

/*
 * Local functions
 */
void ow_interupt(void)
{
	ow_intPending++;
}

void ow_end()
{
	ds.depower();
	pinMode(PIN_OW, INPUT);
	delay(10);
	attachInterrupt(digitalPinToInterrupt(PIN_OW), ow_interupt, FALLING);
}

void ow_start(byte sw, byte cmd, byte end)
{
	detachInterrupt(digitalPinToInterrupt(PIN_OW));
	pinMode(PIN_OW, OUTPUT);
	digitalWrite(PIN_OW, HIGH);
	ds.reset();
	ds.select(addr[sw]);
	ds.write(cmd);
	if (end)
		ow_end();
}

void ow_detect(void) {
	byte i, crc;
#ifdef FAKE_DEV
	byte found;
#else
	byte j;
#endif
	detachInterrupt(digitalPinToInterrupt(PIN_OW));
	pinMode(PIN_OW, OUTPUT);
	digitalWrite(PIN_OW, HIGH);
	ds.reset_search();
	delay (10);
	if (ds.reset() == 0) {
		return;
	}
	delay (10);
	sensors = 0;
	while (ds.search(addr[sensors])) {
		crc = OneWire::crc8(addr[sensors], 7);
		if (crc != addr[sensors][7])
			Log.print(F("CRC is not valid!"));
		sensors++;
	}
#ifdef FAKE_DEV
	const uint8_t at_id[3][8] = { { 0xA2, 0xA2, 0x10, 0x84, 0x00, 0x00, 0x02, 0x27 },
	{ 0xA3, 0x01, 0x03, 0x0, 0x00, 0x00, 0x01, 0x18 },
	{ 0xA8, 0x01, 0x5, 0x0, 0x00, 0x00, 0x01, 0xDE } };
	const uint8_t ds_id1[8] = { 0x28, 0xa3, 0xf9, 0x40, 0x7, 0x00, 0x00, 0xd4 };
	found = 0;
	for (i = 0; i < sensors; i++)
		if (addr[i][7] == at_id[0][7])
			found = 1;
	if (found == 0) {
		memcpy(addr[sensors++], ds_id1, 8);
		for (i = 0; i < 3; i++)
			memcpy (addr[sensors++], at_id[i], 8);
	}
#else
	if (sensors == 0)
		Log.println(F("search failed"));
#endif
	ow_end();
	uint8_t known[][2] = { { 0x27, 1 } };
	for (i = 0; i < sensors; i++)
		if (addr[i][7] == known[i][0]) {
		}
#ifndef FAKE_DEV
	// print sensors
	for (j = 0; j < sensors; j++) {
		Log.printf_P("#%d: %02X %02X .. %02X", j, 
			addr[j][0], addr[j][1], addr[j][7]);
	}
#endif
}

#define OW_READ_SCRATCHPAD 0xBE
void ow_read (byte sw, byte* p_data, size_t cnt) {
	size_t i;

	ow_start (sw, OW_READ_SCRATCHPAD, 0);
	for (i = 0; i < cnt; ++i)
		p_data[i] = ds.read();
	ow_end();
}

void read_temp(byte state, byte sensor) {
	byte data[12];
	int16_t raw;

	if (state == 0) {
		// start conversion and return
		ow_start (sensor, 0x44, 1);
		return;
	}
	// we might do a ds.depower() here, but the reset will take care of it.
	// Read Scratchpad
	ow_read (sensor, data, 9);
	// Convert the data to actual temperature
	raw = (data[1] << 8) | data[0];
	// multiply by (100 * 0.0625) or 6.25
	raw = (6 * raw) + raw / 4;
	sensor_data[sensor].temp[0] = raw / 100;
	sensor_data[sensor].temp[1] = raw % 100;
}

void temp_loop(byte state) {
	int i;

	for (i = 0; i < sensors; i++)
		if (addr[i][0] == 0x28)
			read_temp(state, i);
}

#define OW_WRITE_SCRATCHPAD 0x4E

void sw_irSend (byte sw, unsigned long data) {
	int i;
	union {
		unsigned long l;
		byte b[4];
	} d;
	d.l = data;
	
	ow_start(sw, OW_WRITE_SCRATCHPAD, 0);
	for (i = 0; i < 4; ++i)
		ds.write(d.b[i]);
	ow_start(sw, 0x70, 0);
	switch (data) {
		case IR_ONOFF:
			sensor_data[sw].sw[2] = !sensor_data[sw].sw[2];
			break;
		case IR_VOLUP:
		case IR_VOLDN:
			/* send again to speed up change */
			ow_start(sw, 0x70, 0);
			ow_start(sw, 0x70, 0);
			break;
	}
	ow_end();
}

/**
sw which ow device idx
output which one of the output pins
val 0: off, 1: on
*/
void sw_exec(byte sw, byte out, byte val)
{
	Log.printf_P("switching %d #%d = %d [%02X..%02X]", sw, out, val, addr[sw][0], addr[sw][6] );
	if (addr[sw][0] == 0xA3 && out == 2) {
		sw_irSend(sw, IR_ONOFF);
		return;
	}
	if (val == 0)
		ow_start(sw, 0x47 + out, 1);
	else
		ow_start(sw, 0x67 + out, 1);
	sensor_data[sw].sw[out] = val;
}

/**
	val 0: off, 1: on
*/
void sw_lamp (byte val) {
	if (lamp_sw == val)
		return;
	Log.printf_P ("lamp sw: %d to %d", lamp_sw, val);
	digitalWrite(PIN_LAMP_RELEAIS, !val);
	lamp_sw = val;
	/*
	if (lamp_sw && cfg.telegram)
		bot.sendMessage(CHAT_ID, "Licht Küchentisch an!", "Markdown");
	*/
}

void handleTemp() {
	if (sensors == 0)
		ow_detect ();
	if (sensors == 0)
		return;
	temp_loop(0);
	delay (800);
	temp_loop(1);
}

void evalWebArgs() {
#if 0
	if (server.arg("p") != ""){
		int p, f;
		p = server.arg("p").toInt();
		f = server.arg("f").toInt();
		Log.printf ("setting %d to %d", p, f);
		switch (f) {
		case 0:
			pinMode(p, INPUT);
			break;
		case 1:
			pinMode(p, INPUT_PULLUP);
			break;
		case 2:
			pinMode(p, INPUT_PULLDOWN_16);
			break;
		case 3:
			pinMode(p, OUTPUT_OPEN_DRAIN);
			if (server.arg("v") != "") {
				int v = server.arg("v").toInt();
				digitalWrite (p, v);
			}
			break;
		}
	}
#endif
	if (server.arg("query") != ""){
		if (server.arg("query") == "refresh")
			ow_detect();
		if (server.arg("query") == "dis") {
			setState (ST_INACT_FORCE);
		}
		if (server.arg("query") == "ir") {
			int ir = server.arg("on").toInt();
			sw_irSend(server.arg("sw").toInt(), ir);
		}
		if (server.arg("query") == "cmd") {
			int sw = server.arg("sw").toInt();
			int cmd = strtol(server.arg("on").c_str(), NULL, 16);

			Log.printf_P("cmd %02X to %d [%02X..%02X]", cmd, sw, addr[sw][0], addr[sw][6]);
			ow_start(sw, cmd, 1);
		}
		if (server.arg("query") == "sw") {
			int sw, out = 0;
			const String s = server.arg("sw");
			int point = s.indexOf('.');

			sw = s.substring(0, point++).toInt();
			if (point > 0)
				out = s.substring(point, point + 2).toInt();
			if (server.arg("on") != "") {
				byte val = atoi(server.arg("on").c_str());
				if (sw > 99)
					sw_lamp(val);
				else
					sw_exec(sw, out, val);
			}
		 }
	}
}

void http_redirect () {
	server.send(200, F("text/html"), F("<html><head><meta http-equiv='refresh' content='0;index.html'/></head></html>"));
}

void statusUpdate(String& content)
{
	int i, j, cnt;

	content += "{";
	now = time(nullptr);
	currentTime = localtime(&now);
	if (currentTime) {
		content += "\"time\":\"" +
			String(currentTime->tm_hour) + ":" +
			String(currentTime->tm_min) + ":" +
			String(currentTime->tm_sec) + "\",";
	}
	String s = Log.last();
	if (s.length() > 0)
		content += "\"log\" : \"" + s + "\",";
	content += "\"alarms\":" + String(alarms);
	if (cfg.presence)
		content += ",\"devices\":" + String(active_devs);
	content += ",\"sensor\":[ ";
	cnt = 0;
	for (j = 0; j < sensors; j++) {
		if (addr[j][0] == 0x28) {
			if (cnt++ > 0)
				content += ",";
			content += "{\"id\":\"" + String(j) +
				"\",\"val\":\"" + String(sensor_data[j].temp[0]) +
				"." + sensor_data[j].temp[1] + " C \"}";
		}
	}
	content += "]";
	content += ",\"switch\":[";
	//content += "{ \"id\" : \"100\", \"val\" : " + String(lamp_sw) + " } ";
	cnt = 0;
	for (j = 0; j < sensors; j++) {
		if (addr[j][0] == 0xA2) {
			if (cnt++ > 0)
				content += ",";
			for (i = 0; i < 1; i++) {
				content += "{ \"id\":\"" +
					String(j) + "." + String(i) +
					"\", \"val\":" +
					String(sensor_data[j].sw[i]) + "}";
			}
		}
		if (addr[j][0] == 0xA3) {
			for (i = 0; i < 3; i++) {
				if (cnt++ > 0)
					content += ",";
				content += "{\"id\":\"" +
					String(j) + "." + String(i) +
					"\",\"val\":" +
					String(sensor_data[j].sw[i]) + "}";
			}
		}
	}
	content += "]}\n\n";
}

void handleStatusUpdate(bool event) {
	String content = "";

	if (WiFi.status() != WL_CONNECTED)
		return;
	content.reserve(0x512);
	if (event) {
		//content = " event: status \n";
		content += "data: ";
	}
	statusUpdate(content);
	if (event)
		server.send(200, "text/event-stream", content);
	else
		server.send(200, "text/plain", content);

}

#include <Client.h>
#define HOST "api.telegram.org"
#define SSL_PORT 443
#define maxMessageLength 1300
String sendGetToTelegram(String command) {
	String mess = "";
	long now;
	bool avail;
	bool _debug = true;
	WiFiClientSecure *client = new WiFiClientSecure();

	// Connect with api.telegram.org
	if (client->connect(HOST, SSL_PORT)) {
		if (_debug)
			Log.print(F("Telegram connected to server"));
		String a = "";
		int ch_count = 0;
		client->println("GET /" + command);
		now = millis();
		avail = false;
		while (millis() - now<1500) {
			while (client->available()) {
				char c = client->read();
				if (ch_count < maxMessageLength) {
					mess = mess + c;
					ch_count++;
				}
				avail = true;
			}
			if (avail) {
				if (_debug)
					Log.print(mess);
				break;
			}

		}
	}
	if (!avail)
		Log.print(F("Sending failed"));

	delay(10);
	delete client;
	return mess;
}

bool checkForOkResponse(String response) {
	int responseLength = response.length();

	for (int m = 5; m < responseLength + 1; m++) {
		//Chek if message has been properly sent
		if (response.substring(m - 10, m) == "{\"ok\":true")
			return true;
	}

	return false;
}

bool sendMessage(String chat_id, String text, String parse_mode) {

	long sttime = millis();

	if (text != "") {
		String command = "bot" + String(BOT_TOKEN) +
			"/sendMessage?chat_id=" + chat_id + "&text=" +
			text + "&parse_mode=" + parse_mode;
		while (millis() < sttime + 8000) {
			// loop for a while to send the message
			String response = sendGetToTelegram(command);
			if (checkForOkResponse(response))
				return true;
		}
	}

	return false;
}

void telegramWelcome()
{
#if 0
	String message;
	if (WiFi.status() != WL_CONNECTED)
		return;
	message = "OW Web server ";
	message += VERSION;
	message += " up and running, IP: ";
	IPAddress ip = WiFi.localIP();
	message += ip.toString();
	message.concat("\n");
	if(bot->sendMessage(CHAT_ID, message, "Markdown")){
		Serial.println("TELEGRAM Successfully sent");
	} else
		Serial.println("TELEGRAM sending failed");
	delay(1);
#endif
}

void handleConfigPost() {
	if (server.args() > 0 ) {
		for (uint8_t i = 0; i < server.args(); i++ ) {
			if (server.argName(i) == "presence") {
				if (server.arg(i) == "on") {
					cfg.presence = 1;
					tr64.init();
				} else
					cfg.presence = 0;
			}
		}
	}
	if (server.arg("syslog") != "") {
		IPAddress ip;
		ip.fromString(server.arg("syslog"));
		Log.init(ip);
		Log.println (F("syslog init done"));
	}
	if (server.arg("telegram") == "on") {
		sendMessage(CHAT_ID, "Welcome", "Markdown");
	}
	if (WiFi.status() != WL_CONNECTED) {
		WiFi.mode(WIFI_STA);
		if (!connectWifi(1, 1000))
			WiFi.mode(WIFI_AP_STA);
	}

	if (server.arg("telegram") == "on") {
		if (cfg.telegram == 0) {
			cfg.telegram = 1;
		}
	}
	else {
		if (cfg.telegram == 1) {
			cfg.telegram = 0;
		}
	}
	if (server.arg("store") == "on")
			cfg.writeConfig();
	handleIdx();
}

void handleSensors() {
	String content;
	static char temp[10];
	static char id [8*3+1];
	static byte d[8];
	int i, j;

	evalWebArgs();
	for (j = 0; j < sensors; j++) {
		content += F("<div class=\"line\"><p>");
		content += F(R"=====(<a class="check" id=")=====");
		content += String(j);
		content += F(R"=====(" onclick = "btn_click(this, 'cmd');">#)=====");
		content += String (j);
		content += "</a>";
		id[0] = 0;
		for (i = 0; i < 8; i++) {
			snprintf (temp, sizeof(temp),"%02X", addr[j][i]);
			strcat (id, temp);
		}
		content += String (id);
		content += ") = ";
		ow_read (j, d, sizeof(d));
		id[0] = 0;
		for (i = 0; i < sizeof(d); i++) {
			snprintf (temp, sizeof(temp),"%02X ", d[i]);
			strcat (id, temp);
		}
		content += String (id);
		content += "</div>";
	}
	web.sendSensorsPage(content);
}

void handleIdx() {
	static const char content[] PROGMEM = R"=====(<div id="kueche" class="container">
	<a class="btn" style="display:none" id="2.0" onclick="btn_click(this, 'sw');">Steh</a>
	<a class="btn" style="display:none" id="100" onclick="btn_click(this, 'sw');">Tisch</a>
	<span class="temp" id="0" style="display:none">0.0 C</span>
	</div>
	<div id="wohn" class="container">
		<a class="btn" id="1.0" style="display:none" onclick="btn_click(this, 'sw');">Ecke</a>
	</div>
	<div id="media" class="container" style="display:none">
		<a class="btn" id="2.2" onclick="btn_click(this, 'sw');">Music</a>
		<a class="btn khaki" id="2.1587632295" onclick="btn_click(this, 'ir');"><</a>
		<a class="btn khaki" id="2.1587664935" onclick="btn_click(this, 'ir');">></a>
		<a style="display:none" class="btn" id="2.1" onclick="btn_click(this, 'sw');">LED</a>
	</div>)=====";
	web.sendPage_P("home", content);
	handleStatusUpdate(1);
}

void handleMain() {
#ifdef DYN_INDEX
	int i, j;
	static char url[32];
	static char name[32];
	String data;
	
	if (main_page.length() > 0) {
		web.sendPage(main_page);
		return;
	}
	content = F("<div id = "kueche" class = "container">");
	web.addButton(main_page, "Tisch", 2, "1");
	i = 0;
	for (j = 0; j < sensors; j++) {
		if (addr[j][0] == 0xA2 || addr[j][0] == 0xA3) {
			snprintf(name, sizeof(name), "Licht %d", j + 1);
			snprintf(url, sizeof(url), "%d", (j+1)*10 + i);
			web.addButton(main_page, name, 2, url);
		}
		if (addr[j][0] == 0x28) {
			main_page += "<span class=\"temp\" id=\"" + String((j + 1) * 10) + "\">";
			main_page += String(sensor_data[j].temp[0]) + "." + sensor_data[j].temp[1] + " C";
			/* end of row and of container */
			main_page += "</span>";
		}
	}
	/* end of room */
	main_page += "</div>";
	for (j = 0; j < sensors; j++) {
		if (addr[j][0] == 0xA3) {
			main_page += "<div id=\"media\" class=\"w3-container\">";
			snprintf (url, sizeof(url),"%d", (j + 1) * 10 + 1);
			web.addButton(main_page, "Music", 2, url);
			snprintf (url, sizeof(url),"%d.%d", j, IR_VOLUP);
			web.addButton(main_page, "Up", 0, url);
			snprintf (url, sizeof(url),"%d.%d", j, IR_VOLDN);
			web.addButton(main_page, "Dn", 0, url);
			main_page += "</div><!--media-->";
		}
	}
#endif
}

void handleAction() {
	String data;
	
	if (server.args() == 0)
		return;
	
	evalWebArgs();
	statusUpdate(data);
	server.send(200, "text/html", data);
}

void handleWifi() {
	int i, wlan, numDev;
	String ip, n, mac;
	String content;
	
	tr64.init();
	content.reserve(0x200);
	content += "<div class=\"container\">";
	//content += "<div class=\"panel teal\">Active Devices</div>";
	for (wlan = 1; wlan < 4; ++wlan) {
		numDev = tr64.getWifiDeviceCount(wlan);
		for (i = 0; i < numDev; ++i) {
			if (tr64.getWifiDeviceStatus(wlan, i, &ip, &mac) == -1)
				break;
				if (ip == WiFi.localIP().toString() || ip == "0.0.0.0")
					continue;
			content += tr64.getDeviceName(mac);
			content += " (" + ip + ") [" + wlan + "]<br/>\n";
		}
	}
	content += "</div>";
	web.sendPage("wifi", content, false, false);
}

/* wait if 0, not waiting for connection (trying once). Else wait specifies
				timout in 100 ms steps.
*/
bool connectWifi(char en, int wait) {
	int i = 0;

	if (en == 0) {
		WiFi.disconnect(1);
		return true;
	}
	if (WiFi.status() == WL_CONNECTED)
		return true;
	WiFi.begin(cfg.ssid, cfg.password);
	if (wait > 1) {
		Log.print ("connecting");
		Serial.flush();
	}
	/* trying to connect */
	while (WiFi.status() != WL_CONNECTED) {
			if (wait-- <= 0) {
				if (i > 0) {
					Log.println("failed!");
				}
				return false;
			}
			delay(100);
			if (i++ % 10 == 0) {
				Serial.print (".");Serial.flush();
			}
	}
	WiFi.begin(cfg.ssid, cfg.password);
	setState(ST_ACT);
	Serial.printf ("IP %s\n", WiFi.localIP().toString().c_str());
	return true;
}

void powerDown() {
	int sw, i;
	
	for (sw = 0; sw < sensors; sw++) {
		for (i = 0; i < 2; i++) {
			if ((addr[sw][0] == 0xA3 || addr[sw][0] == 0xA2) &&
				sensor_data[sw].sw[i]) {
				if (addr[sw][0] == 0xA3 && i == 2)
					sw_irSend(sw, IR_ONOFF);
				else
					sw_exec(sw, i, 0);
			}
		}
	}
	sw_lamp(0);
	Log.printf("Powered down!");
	server.stop();
	delay(1000);
	WiFi.disconnect(1);
}

void setState (WifiState s) {
	DBG ("state %d -> %d", state, s);
	switch (s) {
		case ST_INACT_POWERDN:
			powerDown();
			active_poll = millis() + DEADTIME;
			break;
		case ST_ACT_SLEEP:
			Log.print(F("no users, powering down soon"));
			break;
		case ST_ACT_KEEP:
			sleepCountDown = 2 * SLEEP_TO;
			break;
		case ST_INACT_CHECK:
		case ST_ACT:
		case ST_INACT_FORCE:
			break;
	}
	state = s;
}

void logInfo(String &content)
{
	String s[MAX_LOG];
	int cnt, i;

	cnt = Log.read(s);
	for (i = 0; i < cnt; i++)
		content += s[i] + "\n";
}

void webSetup() {
	server.on("/", handleIdx);
	server.on("/config", []() { web.handleConfig(cfg); });
	server.on("/status", []() { handleStatusUpdate(1); });
	server.on("/cfg", HTTP_POST,  handleConfigPost);
	server.on("/action", HTTP_POST, handleAction);
	server.on("/sensor", HTTP_POST, []() {
			evalWebArgs();
			handleSensors(); });
	server.on("/sensors", handleSensors);
	server.on("/index.html", handleIdx);
	server.on("/main", handleMain);
	server.on("/test1", []() { web.sendPage("test", String("test1"), false, false); });
	server.on("/info", []() {
		String content;
		content = VERSION;
		content += "\ncompiled ";
		content += __TIME__;
		content += "\nUp ";
		content += bootTime;
#ifndef WIN32
		content += "\nFree ";
		content += String(ESP.getFreeHeap());
#endif
		logInfo(content);
		server.send(200, "text/plain", content);
	});
	server.on("/log", []() {
		String content;
		logInfo(content);
		server.send(200, "text/plain", content);
	});
	server.on("/refresh", []() {
		ow_detect();
		http_redirect();
	});
	server.on ("/dis", []() {
		http_redirect ();
		setState (ST_INACT_FORCE);
		powerDown();
	} );
#ifdef USE_TR064
	server.on("/wifi", handleWifi);
#endif
	server.on ("/script.js", []() {
		server.send_P( 200, "text/javascript", web_script_js);
	} );
	server.on("/style.css", []() {
		server.send_P(200, "text/css", web_style);
	});
#ifndef WIN32
	server.on("/reset", []() {
		http_redirect();
		ESP.restart();
	});
	httpUpdater.setup(&server);
#endif
	web.setup(&server);
	server.begin();

#ifdef DYN_INDEX
	main_page.reserve(5000);
#endif
}

// (utc+) TZ in hours
#define TZ 1
// use 60mn for summer time in some countries
#define DST_MN	0
#define TZ_SEC ((TZ)*3600)
#define DST_SEC	((DST_MN)*60)
void timeSetup()
{
	unsigned timeout = 10000;
	unsigned start = millis();
	time_t _now;

	while (millis() - start < timeout) {
#ifndef WIN32
		configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
#endif
		_now = time(nullptr);
		if (_now > (2016 - 1970) * 365 * 24 * 3600) {
			delay(50);
			now = time(nullptr);
			currentTime = localtime(&now);
			sprintf(bootTime, "%02d.%02d %02d:%02d",
				currentTime->tm_mday, currentTime->tm_mon + 1,
				currentTime->tm_hour, currentTime->tm_min);
			Log.printf_P("boot time %s", bootTime);
			return;
		}
		delay(100);
	}
	Log.print(F("Time failed"));
}

void setup(void)
{
	int devs;
	
	Serial.begin(115200);
	Log.init(35);
	Serial.printf_P(PSTR("\n\nstarting ... %s"), __TIME__);
	delay (10);
	cfg.readConfig();
	// pin setup
	pinMode(PIN_LAMP_RELEAIS, OUTPUT_OPEN_DRAIN);
	pinMode(PIN_LAMP_SW, INPUT_PULLUP);
	digitalWrite(PIN_LAMP_RELEAIS, HIGH);
	delay(10);
	lamp_st = digitalRead(PIN_LAMP_SW);
	WiFi.mode(WIFI_STA);
	delay (10);
	// connecting....timout 1 mins
	if (!connectWifi(1, 1 * 60 * 10)) {
		cfg.presence = 0;
		cfg.telegram = 0;
		WiFi.mode(WIFI_AP_STA);
	}
#ifdef MDNS
	MDNS.begin("esp");
	delay (100);
	MDNS.addService("http", "tcp", 80);
#endif
	state = ST_ACT;
	if (cfg.presence == 1) {
		tr64.init();
		devs = tr64.getWifiDevicesStatus(true);
	} else
		delay(500);
	delay(500);
	ow_detect ();
	if (sensors == 0) {
		/* try again after some more wait time */
		delay(500);
		ow_detect ();
	}
	ow_intPending = 0;
	timeSetup();
	webSetup();
	//Bot_lasttime = millis();
	DBG("Setup done");
	if (cfg.telegram) {
		sendMessage(CHAT_ID, "Welcome", "Markdown");
	}
}

int getDevicesStatus() {
	int devs;
#ifdef USE_TR064
	if (cfg.presence == 1) {
		 devs = tr64.getWifiDevicesStatus(false);
		 if (devs != active_devs) {
			 /* ignore me */
			 active_devs = devs - 1;
			 for (int wlan = 1; wlan < 4; ++wlan) {
				 int numDev = tr64.getWifiDeviceCount(wlan);
				 for (int i = 0; i < numDev; ++i) {
					String ip;
					if (tr64.getWifiDeviceStatus(wlan, i, &ip, NULL) == -1)
						break;
					if (currentTime->tm_hour > 18 && currentTime->tm_hour < 5
						&& ip == "192.168.178.31") {
						sw_exec(2, 0, 1);
					}
				 }
			 }
			 handleStatusUpdate(1);
		 }
	} else
		active_devs = 1;

	return active_devs;
#endif
}

void activePoll()
{
	if (millis() < active_poll)
		return;
	active_poll = millis() + ACT_POLL_TIME;
	if (currentTime == NULL)
		timeSetup();
	now = time(nullptr);
	currentTime = localtime(&now);
	switch (state) {
	case ST_INIT:
		setState(ST_ACT);
		break;
	case ST_ACT_KEEP:
		sleepCountDown--;
		if (sleepCountDown == 0)
			setState(ST_ACT);
		break;
	case ST_ACT:
		// gone ?
		if (WiFi.status() != WL_CONNECTED) {
			setState(ST_INACT_CHECK);
			return;
		}
		switch (getDevicesStatus()) {
		case 0:
			setState(ST_ACT_SLEEP);
			sleepCountDown = SLEEP_TO;
			break;
		case -1:
			// error, we need to poll again and just do nothing for now
			if (WiFi.status() != WL_CONNECTED) {
				DBG("no wifi!");
				setState(ST_INACT_CHECK);
			}
			break;
		default:
			// connected users
			break;
		}
		break;
	case ST_ACT_SLEEP:
		// active but no users, so powering down soon
		if (WiFi.status() != WL_CONNECTED) {
			setState(ST_INACT_CHECK);
			return;
		}
		// users gone?
		switch (getDevicesStatus()) {
		case 0:
		case -1:
			/* no users or problem reading the state */
			if (currentTime->tm_hour > 22) {
				// bedtime, force power down
				Log.print(F("no users, bed time"));
				setState(ST_INACT_POWERDN);
			}
			else {
				sleepCountDown--;
				if (sleepCountDown == 0) {
					Log.print(F("Sleeping"));
					setState(ST_INACT_CHECK);
					//WiFi.disconnect(1);
				}
			}
			break;
		default:
			/* connected users or presence detection switched off 
			if (currentTime->tm_hour > 17 && currentTime->tm_hour < 5)
			sw_lamp(1); */
			setState(ST_ACT);
			Log.print(F("Wake-up"));
			break;
		}
	case ST_INACT_FORCE:
		/* Immediately forcing power down in this loop */
		setState(ST_INACT_POWERDN);
		break;
	case ST_INACT_POWERDN:
		// forcing power down with a dead time (not checking for new connection)
		// first after switching off and nobody is connected,
		// wait longer to let Fritz switch WLAN off
		setState(ST_INACT_CHECK);
		break;
	case ST_INACT_CHECK:
		// inactive, but checking for Wifi connection
		//DBG("checking wifi");
		if (!connectWifi(1, 0))
			return;
		switch (getDevicesStatus()) {
		case 0:
			// if in rest time, switch off again if nobody is active
			//powerDown();
			setState(ST_ACT_SLEEP);
			break;
		case -1:
			// error, we need to poll again and just do nothing for now
			break;
		default:
			server.begin();
			Log.print(F("connected users"));
			// connected users
			if (currentTime->tm_hour > 18 && currentTime->tm_hour < 21)
				sw_lamp(1);
			setState(ST_ACT);
			break;
		}
		break;
	}
}

void loop(void){
	static unsigned long status_poll = 0;

	if (digitalRead(PIN_LAMP_SW) != lamp_st) {
		delay(1);
		lamp_st = digitalRead(PIN_LAMP_SW);
		// toggle
		//sw_lamp(!lamp_sw);
		int j;
		for (j = 0; j < sensors; j++) {
			if (addr[j][0] == 0xA2) {
				sw_exec(j, 0, !sensor_data[j].sw[0]);
				break;
			}
		}
		DBG("lamp switched!");
		String s;
		Serial.println (s);
		Serial.printf("Wifi.status = %d\n", WiFi.status());
		Serial.printf("state = %d\n", state);
		Serial.printf("Client state = %d\n", server.client().status());
#ifndef WIN32
		Serial.printf("Mem = %d\n", ESP.getFreeHeap());
#endif
		// switch wifi forcible on
		if (connectWifi(1, 1000)) {
			server.begin();
			setState(ST_ACT);
			handleStatusUpdate(1);
		}
	}
	if (ow_intPending) {
		Log.print("Interrupt!");
		//sw_exec(2, 0, 1);
		handleStatusUpdate(1);
		alarms++;
		ow_intPending = 0;
	}
	if (WiFi.status() == WL_CONNECTED)
		server.handleClient();
	/*
	 * Active Wifi devices polling
	 */
	activePoll();
	/*
	 * Sensors polling 
	 */
	if (millis() > status_poll) {
			static byte tl = 0;
			if (sensors == 0)
				ow_detect ();
			if (tl == 0) {
				status_poll = millis() + 1000;
			}
			temp_loop(tl++);
			if (tl > 1) {
				//DBG("Temperature read");
				status_poll = millis() + STATUS_POLL_CYCLE;
				/* done */
				tl = 0;
				handleStatusUpdate(1);
			}
	}
}
