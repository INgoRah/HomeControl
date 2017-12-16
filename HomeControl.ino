/*
 * Global switchs
 */
#define USE_TR064
#define NO_FAKE_DEV

/*
 * Includes
 */
#include <time.h>
#include <sys/time.h>                   // struct timeval
#include <ESP8266WiFi.h>
#include "FS.h"

/*
 * Library classs includes
 */
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUDP.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <OneWireX.h>
#include <SysLogger.h>
#ifdef USE_TR064
#include "tr064.h"
#else
#include <ESP8266Ping.h>
#endif /* USE_TR064 */

/*
 * Local Includes
 */
#include "HomeControl.h"
#include "HomeWeb.h"
#include "HomeConfig.h"
#include "config.h"

/*
 * Local constants
 */
const char* host = "esp";
const static char* days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
/** standard LED, used on breadboard */
const char led = 2;

/** output for direct connected relais, low active */
#define PIN_LAMP_RELEAIS 10
/*  input of direct connected wall switch, active low */
#define PIN_LAMP_SW 9
/** OneWire interface pin */
#define PIN_OW 4
/** check interval for temperature e.g. */
#define STATUS_POLL_CYCLE (1000 * 60)
/** time to wait for new connection after switching off Wifi. Let FritzBox
  switch off its Wifi in inactive times */
#define DEADTIME (60 * 60 * 1000)
#define SLEEP_TO 2
/** time for polling interval on in/active devices */
#define ACT_POLL_TIME (1000 * 15)
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
  ST_ACT,
  // active but no users, so powering down soon
  ST_ACT_SLEEP,
  // forcing power down with a dead time (not checking for new connection)
  ST_INACT_FORCE,
  // inactive, but checking for Wifi connection
  ST_INACT_CHECK,
};

class StateLog {
    private:
      int cnt;
      struct entry {
          struct tm* tm;
          enum WifiState state;
      } log[20];
};

/*
 * Objects
 */
ESP8266WebServer server(80);
// SSL client needed for both libraries
WiFiClientSecure client;
ESP8266HTTPUpdateServer httpUpdater;
// on pin PIN_OW (a max 4.7K resistor is necessary)
OneWire  ds(PIN_OW);
volatile boolean ow_intPeding;
#ifdef USE_TR064
TR064 tr64(PORT, fritz_ip, fritz_user, fritz_passwd);
#endif /* USE_TR064 */
UniversalTelegramBot bot(BOT_TOKEN, client);
HomeWeb web;
HomeConfig cfg;
StateLog stateLog;
SysLogger Log;

/*
 * Local variables
 */
static int lamp_st = 0;
static int lamp_sw = 0;
static unsigned long deadtime = 0;
static unsigned long active_poll = 0;
static int sleep_to = SLEEP_TO;
static WifiState state;
static struct tm* boot_tm;
int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;   //last time messages' scan has been done

static byte addr[MAX_SENSORS][8];
static byte sensors;

/* each switch contains two outputs (0=PowerLine, 1=LED), for temp it contains the 16 temp */
static union {
  byte temp[2];
  byte sw[2];
}sensor_data[MAX_SENSORS];

/*
 * Local functions
 */
void ow_interupt(void)
{
  ow_intPeding++;
}


void CheckFlash()
{
    uint32_t realSize = ESP.getFlashChipRealSize();
    uint32_t ideSize = ESP.getFlashChipSize();
    FlashMode_t ideMode = ESP.getFlashChipMode();
#if 0
    Serial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
    Serial.printf("Flash real size: %u\n\n", realSize);

    Serial.printf("Flash ide  size: %u\n", ideSize);
    Serial.printf("Flash ide speed: %u\n", ESP.getFlashChipSpeed());
    Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
#endif
    if(ideSize != realSize) {
        Serial.println("Flash Chip configuration wrong!\n");
    } else {
        Serial.println("Flash Chip configuration ok.\n");
    }
}

void ow_start(byte sw, byte cmd, byte end)
{
  detachInterrupt(digitalPinToInterrupt(PIN_OW));
  ds.reset();
  ds.select(addr[sw]);
  ds.write(cmd);
  if (end) {
    ow_intPeding = 0;
    attachInterrupt(digitalPinToInterrupt(PIN_OW), ow_interupt, LOW);
  }
}

void ow_end()
{
  ow_intPeding = 0;
  attachInterrupt(digitalPinToInterrupt(PIN_OW), ow_interupt, LOW);
}

void ow_detect(void) {
  byte i, crc;
#ifdef FAKE_DEV
  byte found;
#else
  byte j;
#endif

  detachInterrupt(digitalPinToInterrupt(PIN_OW));
  ds.reset_search();
  delay (1);
  if (ds.reset() == 0) {
      return;
  }
  sensors = 0;
  while (ds.search(addr[sensors])) {
    crc = OneWire::crc8(addr[sensors], 7);
    if (crc != addr[sensors][7]) {
      Serial.println("CRC is not valid!");
    }
    sensors++;
  }
#ifdef FAKE_DEV
  const uint8_t at_id1[8] = { 0xA2, 0xA2, 0x10, 0x84, 0x00, 0x00, 0x02, 0x27 };
  const uint8_t at_id2[8] = { 0xA3, 0xA2, 0x10, 0x84, 0x00, 0x00, 0x02, 0x27 };
  const uint8_t ds_id1[8] = { 0x28, 0xA2, 0x10, 0x84, 0x00, 0x00, 0x02, 0x27 };
  found = 0;
  for (i = 0; i < sensors; i++)
    if (addr[i][7] == at_id1[7])
      found = 1;
  if (found == 0) {
    memcpy (addr[sensors++], at_id1, 8);
    memcpy(addr[sensors++], ds_id1, 8);
    memcpy(addr[sensors++], at_id2, 8);
  }
#else
  if (sensors == 0)
	  Serial.println("search failed");
#endif
  ow_end();
#ifndef FAKE_DEV
  // print sensors
  for (j = 0; j < sensors; j++) {
    Serial.print("#");
    Serial.print(j);
    Serial.print(":");
    for( i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[j][i], HEX);
    }
    Serial.println();
  }
#endif
}

#define OW_READ_SCRATCHPAD 0xBE
void ow_read (byte sw, byte* p_data, size_t cnt) {
  size_t i;
  ow_start (sw, OW_READ_SCRATCHPAD, 0);
  for (i = 0; i < cnt; ++i) {
    p_data[i] = ds.read();
    Serial.print(p_data[i], HEX);
    Serial.print(" ");
  }
  ow_end();
  Serial.println();
}


#define DS18_DBG 0
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

  raw = (6 * raw) + raw / 4;    // multiply by (100 * 0.0625) or 6.25
  sensor_data[sensor].temp[0] = raw / 100;
  sensor_data[sensor].temp[1] = raw % 100;
}

void temp_loop(byte state) {
  int i;

  for (i = 0; i < sensors; i++) {
      if (addr[i][0] == 0x28) {
        read_temp(state, i);
      }
  }
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

  for (i = 0; i < 4; ++i) {
    ds.write(d.b[i]);
  }
  ow_start(sw, 0x70, 0);
  switch (data) {
    case IR_ONOFF:
      sensor_data[sw].sw[0] = !sensor_data[sw].sw[0];
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
void sw_exec(byte sw, byte output, byte val)
{
  sw = sw / 10 - 1;
	Log.printf("switching %d #%d = %d\n", sw, output, val);
	if (addr[sw][0] == 0xA3) {
		sw_irSend(2, IR_ONOFF);
		return;
	}
	if (output == 0) {
		if (val == 0)
			ow_start(sw, 0x47, 1);
		else
			ow_start(sw, 0x67, 1);
	}
	else {
		if (val == 0)
			ow_start(sw, 0x48, 1);
		else
			ow_start(sw, 0x68, 1);
	}
	sensor_data[sw].sw[output] = val;
}

/**
  val 0: off, 1: on
*/
void sw_lamp (byte val) {
  Log.printf ("lamp sw: %d to %d\n", lamp_sw, val);
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

void web_evalArgs() {
  if (server.arg("p") != ""){
    int p, f;
    p = server.arg("p").toInt();
    f = server.arg("f").toInt();
    Log.printf ("setting %d to %d\n", p, f);
    switch (f) {
      case 0:
        pinMode(p, INPUT);
        break;
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
  if (server.arg("query") != ""){
    int sw;
    
    sw = server.arg("sw").toInt();
    Log.println (server.arg("query"));
    if (server.arg("query") == "ir") {
      int ir = server.arg("on").toInt();
      Serial.println(ir);
      sw_irSend(2, ir);
    }
    if (server.arg("query") == "sw") {
        int out = 0;
    
        if (server.arg("out") != "")
           out = atoi(server.arg("out").c_str());
        if (server.arg("on") != "") {
          byte val = atoi(server.arg("on").c_str());
          if (sw < 10)
            sw_lamp(val);
          else
            sw_exec(sw, 0, val);
        }
     }
  }
}

void http_redirect () {
	server.send(200, "text/html", "<html><head></head><body>nothing</body></html>");
	//server.send ( 200, "text/html", "<html><head><meta http-equiv='refresh' content='0;index.html'/></head></html>");
}

void handleConfig() {
  String content;
  web.handleConfig(cfg.presence, cfg.telegram, content);
  server.send (200, "text/html", content.c_str());
}

void statusUpdate(String& content)
{
	struct tm* tm;
	time_t now;
	int j;

	now = time(nullptr);
	tm = localtime(&now);
	/*
	*{"time":"21:36:29","alarms":6,"devices":-1,"switches":3,"switch":[{"id":"1","val":1},{"id":"10","val":1},{"id":"20","val":0}]}
	content += "{ \"time\" : " + String(tm->tm_hour) + ":" + String(tm->tm_min) + ":" + String(tm->tm_sec) ;
	*/
  content += "{ \"time\" : \"" + String(tm->tm_hour) + ":" + String(tm->tm_min) + ":" + String(tm->tm_sec) + "\"";
  content += ", \"alarms\" : " + String(ow_intPeding++);
  content += ", \"devices\" : " + String(1);
  content += ", \"sensor\" : [ ";
  int i = 0;
  for (j = 0; j < sensors; j++) {
	  if (addr[j][0] == 0x28) {
		  if (i++ > 0)
			  content += ",";

		  content += " { \"id\" : " + String((j + 1) * 10) +
			  ", \"val\" : \"" + String(sensor_data[j].temp[0]) +
			  "." + sensor_data[j].temp[1] + " C \"} ";
	  }
  }
  content += " ]";
  content += ", \"switch\" : [ ";
  content += "{ \"id\" : \"1\", \"val\" : " + String(lamp_sw) + " } ";
  for (j = 0; j < sensors; j++) {
	  if (addr[j][0] == 0xA2 || addr[j][0] == 0xA3)
		  content += ", { \"id\" : " + String((j+1) * 10) + ", \"val\" : " + String(sensor_data[j].sw[0]) + " } ";
  }
  content += " ] }\n\n";
}

void handleStatusUpdate(bool event) {
  String content;
  
  if (event) {
    //content = " event: status \n";
    content += "data: ";
  }
  statusUpdate(content);
  if (event)
    server.send(200, "text/event-stream", content.c_str());
  else
    server.send(200, "text/plain", content.c_str());
}

void handleStatus() {
  handleStatusUpdate(1);
}

void telegramWelcome()
{
  String message;
  message = "OW Web server up and running, IP: ";
  IPAddress ip = WiFi.localIP();
  message += ip.toString();
  message.concat("\n");
  if(bot.sendMessage(CHAT_ID, message, "Markdown")){
    Serial.println("TELEGRAM Successfully sent");
  } else
    Serial.println("TELEGRAM sending failed");
  delay(1);
}

void handleConfigPost() {
  Serial.println ("handle post");

  if (server.args() > 0 ) { // Are there any POST/GET Fields ?
     Serial.println ("Args=" + server.args());
     for (uint8_t i = 0; i < server.args(); i++ ) {  // Iterate through the fields
          if (server.argName(i) == "telegram") {
            if (server.arg(i) == "on") {
              cfg.telegram = 1;
              telegramWelcome();
            } else {
              cfg.telegram = 0;
            }
          }
          if (server.argName(i) == "presence") {
            if (server.arg(i) == "on") {
              cfg.presence = 1;
              tr64.init();
              tr64.getWifiDevicesStatus(true);
            } else {
              cfg.presence = 0;
            }
          }
      }
  }
#if 1
  IPAddress ip(192,168,178,25);
  Log.init(ip);
  /*
  Log.println ("syslog init done");
  */
#endif
  cfg.writeConfig();
  handleIdx();
}

void handleSensorPost() {
  Serial.println ("handle post");
  Serial.println (server.args());

  handleSensors();
}

void handleSensors() {
#if 0
  static char temp[500];
  char id [8*3+1];
  int i, j;
  byte data[8];
  int cmd = 0x67;
  String content;

  web.startPage(content, 0);
  if (server.args() > 0 ) { // Are there any POST/GET Fields ?
    int sw = (byte)server.arg("id").toInt();
    /*data = server.arg("data").toInt();*/
    if (server.arg("read").compareTo("on")) {
        ow_read(sw, data, 8);
    } else {
      cmd = server.arg("cmd").toInt();
      Log.printf ("CMD 0x%02X to %d\n", cmd, sw);
      ow_start(sw, cmd, 1);
    }
  }
  // sensors
  web.startSection(content, "sensors");
  for (j = 0; j < sensors; j++) {
    content += String ("<div class=\"w3-row\"><div class=\"w3-col w3-container\">");
    snprintf (temp, sizeof(temp),"<p>#%d (", j);
    content += String (temp);
    id[0] = 0;
    for( i = 0; i < 8; i++) {
      snprintf (temp, sizeof(temp),"%02X", addr[j][i]);
      strcat (id, temp);
    }
    content += String (id);
    content += ") ";
    content += R"====(<form class="w3-container" action="sensor" method="POST">)====";
    snprintf (temp, sizeof(temp), "<input type='hidden' value='%d' name='id'>", j);
    content += temp;
    ow_read(j, data, 8);
    for (i = 0; i < 8; ++i) {
      snprintf (temp, sizeof(temp)," %02X", data[i]);
      content += String (temp);
    }
    content += String ("<br>cmd <input type='text' name='cmd' size='4'");
    snprintf (temp, sizeof(temp)," 0x%02X", cmd);
    content += String (temp);
    content += String ("> data <input type='text' name='data' size='10'>");
    content += String ("<input type='checkbox' name='read' checked> Read ");
    content += String (R"====(<input type="submit" value="Send" class="w3-btn w3-round-large w3-small w3-blue-grey" type="button"></form>)====");
    /* end of row and container */
    content += String ("</div></div><!--container-->");
  }
  /* end of sensors */
  content += String ("</div><!--sensors-->");
  web.endPage(content);
  server.send (200, "text/html", content.c_str());
#else
	http_redirect();
#endif
}

void handleIdx() {
	int i, j;
	static char url[32];
	static char name[32];
	String content, data;
	
	statusUpdate(data);
	web.startPage(content, data);
	web.startSection (content,"kueche");
	web.addButton(content, "Tisch", 2, "1");
	i = 0;
	for (j = 0; j < sensors; j++) {
		if (addr[j][0] == 0xA2) {
			snprintf(name, sizeof(name), "Licht %d", j + 1);
			snprintf(url, sizeof(url), "%d", (j+1)*10 + i);
			web.addButton(content, name, 2, url);
    		}
		if (addr[j][0] == 0x28) {
      			content += "<div class=\"w3-row\"><div class=\"w3-col w3-container\">";
			content += "<h4 class=\"w3-text-red\" id=\"" + String((j + 1) * 10)  + "\">";
			content += String(sensor_data[j].temp[0]) + "." + sensor_data[j].temp[1] + " C";
      			/* end of row and of container */
      			content += "</h4></div></div>";
    		}
  }
  /* end of room */
	web.endSection(content);
	web.startSection(content, "wohnzimmer");
	web.endSection(content);
  for (j = 0; j < sensors; j++) {
    if (addr[j][0] == 0xA3) {
      content += "<div id=\"media\" class=\"w3-container\">";
			snprintf (url, sizeof(url),"%d", (j + 1) * 10);
			web.addButton(content, "Music", 2, url);
      snprintf (url, sizeof(url),"%d.%d", j, IR_VOLUP);
      web.addButton(content, "Up", 0, url);
      snprintf (url, sizeof(url),"%d.%d", j, IR_VOLDN);
      web.addButton(content, "Dn", 0, url);
      content += "</div><!--media-->";
    }
  }
#if 0    
  /* status */
  content += "<div id=\"devices\" class=\"w3-container\">";
  content += "<div class=\"w3-panel w3-teal\">Active Devices</div>";

  if (server.arg("devs") != "") {
	  int numDev;
	  String ip, n;
	  int active;
	  numDev = tr64.getDeviceCount();
	  for (j = 0; j < numDev; j++) {
	    tr64.getDeviceStatus(j, &ip, &n, &active);
	    if (active && !(ip == WiFi.localIP().toString())) {
	      snprintf (temp, sizeof(temp), "%s (%s)<br/>", n.c_str(), ip.c_str());
	      content += temp;
	    }
	  }
  } else {
    content += String("deactivated");
	  for (j = 0; j < active_devs; j++) {
	      snprintf (temp, sizeof(temp), "%s (%s)<br/>", devs_ip[j].toString().c_str());
	      content += String(temp);
	  }
  }
  content += "</div><!--devs-->";
#endif
	web.endPage(content, ow_intPeding);
	server.send(200, "text/html", content.c_str());
}

void handleAction() {
  String data;
  
  Serial.println ("handle action");
  if (server.args() > 0 ) { // Are there any POST/GET Fields ?
    web_evalArgs();
  }
  statusUpdate(data);
  server.send(200, "text/html", data.c_str());
}

void handleWifi() {
  int i, wlan, numDev;
  String ip, n, mac;
  int active;
  String content;
  
  tr64.init();
  content.reserve(0x500);
  web.startPage(content);
  content += "<div id=\"devices\" class=\"w3-container\">";
  content += "<div class=\"w3-panel w3-teal\">Active Devices</div>";
  for (wlan = 1; wlan < 4; ++wlan) {
    numDev = tr64.getWifiDeviceCount(wlan);
    for (i = 0; i < numDev; ++i) {
      if (tr64.getWifiDeviceStatus(wlan, i, &ip, &mac) == -1)
        break;
        if (ip == WiFi.localIP().toString() || ip == "0.0.0.0")
          continue;
      content += tr64.getDeviceName(mac);
      //content += n;
      content += " (" + ip + ")<br/>\n";
    }
  }
  content += "</div></div>";
  web.endPage(content, ow_intPeding);
  server.send (200, "text/html", content.c_str());
}

/* wait if 0, not waiting for connection (trying once). Else wait specifies
        timout in 100 ms steps.
*/
void wifi_connect(char en, int wait) {
  int i = 0;

  if (en == 0) {
    WiFi.disconnect(1);
    return;
  }
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(cfg.ssid, cfg.password); 
  if (wait != 0) {
    Log.print ("connecting");
    Serial.flush();
  }
  /* trying to connect */
  while (WiFi.status() != WL_CONNECTED) {
      if (wait-- <= 0) {
        if (i > 0)
          Log.println("failed!");
        return;
      }
      delay(100);
      if (i++ % 10 == 0) {
        Serial.print (".");Serial.flush();
      }
      //WiFi.begin(ssid, password);
  }
  Serial.printf ("IP %s\n", WiFi.localIP().toString().c_str());
}

// pins 6 and 11 not working as output...maybe not as input? Why??
void pin_status()
{
  Log.printf (" 1=%d\t", digitalRead(1));
  Log.printf (" 9=%d\n", digitalRead(9));
  Log.printf ("10=%d\t", digitalRead(10));
  Log.printf ("16=%d\n", digitalRead(16));
}

void powerDown() {
  int j, i;
  for (j = 0; j < sensors; j++) {
    switch (addr[j][0]) {
    case 0xA2:
      for (i = 0; i < 2; i++)
        sw_exec(j, i, 0);
      break;
    case 0xA3:
      if (sensor_data[j].sw[0]) {
           sw_irSend (j, IR_ONOFF);
      }
      break;
    }
  }
  sw_lamp(0);
  delay(500);
  wifi_connect(0, 0);
  //Log.println ("Powered down!");
}

void setState (WifiState s) {
  Log.printf ("state %d -> %d\n", state, s);
  switch (s) {
    case  ST_INACT_FORCE:
      if (cfg.telegram)
        bot.sendMessage(CHAT_ID, "going to sleep", "Markdown");
      deadtime = millis();
      break;
    case ST_ACT_SLEEP:
      if (cfg.telegram)
        bot.sendMessage(CHAT_ID, "users gond, wait for sleep", "Markdown");
      sleep_to = SLEEP_TO;
      active_poll = millis();
      break;
    case ST_ACT:
      active_poll = millis();
      break;
    case ST_INACT_CHECK:
      active_poll = millis();
      break;
  }
  state = s;
}

void handleFlash()
{
  CheckFlash();
  // Start filing subsystem
  if (SPIFFS.begin()) {
      Serial.println("SPIFFS Active");
      if (SPIFFS.exists("script.js")) {
        File f = SPIFFS.open("script.js", "r");
        if (!f)
          Serial.println("Unable To Open");
        else {
          String s;
          Serial.println("file opened");
#if 1
          while (f.position() < f.size())
          {
            s=f.readStringUntil('\n');
            s.trim();
            Serial.println(s);
          }
#endif
          f.close();
        }
      }
  } else {
      Serial.println("Unable to activate SPIFFS");
  }
  http_redirect();
}

void webSetup() {
  server.on("/", handleIdx);
  server.on("/config", handleConfig);
  server.on("/status", handleStatus);
  server.on("/cfg", HTTP_POST, handleConfigPost);
  server.on("/action", HTTP_POST, handleAction);
  server.on("/sensor", HTTP_POST, handleSensorPost);
  server.on("/sensors", handleSensors);
  server.on("/index.html", handleIdx);
  server.on("/refresh", []() {
    http_redirect();
    ow_detect();
    handleTemp();
  } );
  server.on ( "/pins", []() {
      pin_status();
      http_redirect();
  } );
  server.on ( "/dis", []() {
      http_redirect ();
      powerDown();
      setState (ST_INACT_FORCE);
  } );
  server.on("/flash", handleFlash);
#ifdef USE_TR064
  server.on("/wifi", handleWifi);
#endif
  server.on ( "/script.js", []() {
    server.send ( 200, "text/plain", web_script_js );
  } );
  httpUpdater.setup(&server);
  web.init(&server);
}

#define TZ              1       // (utc+) TZ in hours
#define DST_MN          0      // use 60mn for summer time in some countries
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
void timeSetup()
{
  timeval tv;
  timespec tp;  
  time_t now;

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  Serial.println("Waiting for time");
  unsigned timeout = 2000;
  unsigned start = millis();
  while (millis() - start < timeout) {
      now = time(nullptr);
      if (now > (2016 - 1970) * 365 * 24 * 3600)
          break;
      delay(25);
  }
  boot_tm = localtime(&now);
  Serial.printf ("%02d:%02d:%02d %s\n", boot_tm->tm_hour, boot_tm->tm_min, boot_tm->tm_sec, days[boot_tm->tm_wday]);
}

void setup(void){
  
  Serial.begin(115200);
  Serial.printf("\n\nstarting up... %s\n", __TIME__);
  delay (10);
  cfg.readConfig();
  WiFi.mode(WIFI_STA);
  delay (10);
  // connecting....timout 5 mins
  wifi_connect(1, 5*60*10);
  MDNS.begin(host);
  delay (100);

  MDNS.addService("http", "tcp", 80);
  webSetup();
  // pin setup
  pinMode(PIN_LAMP_RELEAIS, OUTPUT_OPEN_DRAIN);
  pinMode(PIN_LAMP_SW, INPUT_PULLUP);
  pinMode(PIN_OW, OUTPUT);
  digitalWrite(PIN_OW, HIGH);
  digitalWrite(PIN_LAMP_RELEAIS, HIGH);
  delay (10);
  lamp_st = digitalRead(PIN_LAMP_SW);
  timeSetup();
  ow_detect ();
  ow_intPeding = 0;
  attachInterrupt(digitalPinToInterrupt(PIN_OW), ow_interupt, LOW);
  state = ST_ACT;
  if (cfg.presence == 1) {
    tr64.init();
    tr64.getWifiDevicesStatus(true);
  }
  delay(1000);
  if (cfg.telegram)
    telegramWelcome();
  Bot_lasttime = millis();
}

int getDevicesStatus() {
#ifdef USE_TR064
  if (cfg.presence == 1)
     return tr64.getWifiDevicesStatus(false);
  else
    return 1;
#else
  int i, j;
  int devs = 0;

  for (j = 178; j < 180; j++)
    for (i = 20; i < 40; i++) {
        IPAddress ip(192,168,j,i);
        if(Ping.ping(ip, 1)) {
          devs_ip[devs] = ip;
          devs ++;
        }
    }

  active_devs = devs;
  return devs;
#endif
}

void handleNewMessages(int numNewMessages) {
  //Serial.println("handleNewMessages");
  //Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";
    //Serial.println(text);

    if (text == "Licht") {
        bot.sendChatAction(chat_id, "typing");
        sw_lamp(1);
        // You can't use own message, just choose from one of bellow
        //typing for text messages
        //upload_photo for photos
        //record_video or upload_video for videos
        //record_audio or upload_audio for audio files
        //upload_document for general files
        //find_location for location data
        //more info here - https://core.telegram.org/bots/api#sendchataction
    }
  }
}

void botLoop() {
  if (cfg.telegram == 0)
    return;
  if (millis() > Bot_lasttime + Bot_mtbs)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    Bot_lasttime = millis();
  }
}

void loop(void){
  struct tm* tm;
  time_t now;
  static unsigned long status_poll = 0;
  static unsigned long alarm_to = 0;

  server.handleClient();
  now = time(nullptr);
  tm = localtime(&now);
  if (digitalRead(PIN_LAMP_SW) != lamp_st) {
    delay(1);
    lamp_st = digitalRead(PIN_LAMP_SW);
    // toggle
    sw_lamp(!lamp_sw);
    // switch wifi forcible on
    if (WiFi.status() != WL_CONNECTED) {
      wifi_connect(1, 1000);
      setState (ST_ACT);
    }
    handleStatusUpdate(1);
  }
  botLoop();
  switch (state) {
    case ST_ACT:
      if ((millis() - active_poll) > ACT_POLL_TIME) {
        active_poll = millis();
        // gone ?
        switch (getDevicesStatus()) {
            case 0:
              setState (ST_ACT_SLEEP);
              Log.println("no users, powering down soon");
            break;
            case -1:
              // error, we need to poll again and just do nothing for now
            if (WiFi.status() != WL_CONNECTED) {
              Log.println("no wifi!");
              setState (ST_INACT_CHECK);
            }
            break;
            default:
              // connected users
              break;
          }
      }
      break;
    case ST_ACT_SLEEP: // 1
      // active but no users, so powering down soon
      if ((millis() - active_poll) > ACT_POLL_TIME) {
        if (WiFi.status() != WL_CONNECTED) {
          setState (ST_INACT_CHECK);
          break;
        }
        active_poll = millis();
        // users gone?
        switch (getDevicesStatus()) {
          case 0:
          case -1:
            if (tm->tm_hour > 22) {
              // bedtime, force power down
              Log.println("no users, bed time");
              setState (ST_INACT_FORCE);
              powerDown();
            }
            else {
              Log.printf("no users (%d)\n", sleep_to);
              if (sleep_to-- == 0) {
                setState (ST_INACT_CHECK);
                powerDown();
              }
            }
            break;
            default:
              // connected users
              if (tm->tm_hour > 17 && tm->tm_hour < 5)
                sw_lamp(1);
              setState (ST_ACT);
              bot.sendMessage(CHAT_ID, "Wake-up", "Markdown");
              break;
        }
      }
      break;
    case ST_INACT_FORCE:
      // forcing power down with a dead time (not checking for new connection)
      // first after switching off and nobody is connected,
      // wait longer to let Fritz switch WLAN off
      if (((millis() - deadtime) > DEADTIME)) {
        setState (ST_INACT_CHECK);
      }
      break;
    case ST_INACT_CHECK:
      // inactive, but checking for Wifi connection
      if ((millis() - active_poll) > ACT_POLL_TIME) {
        active_poll = millis();
        Log.println("checking wifi");
        wifi_connect(1, 0);
        if (WiFi.status() == WL_CONNECTED) {
          Log.println("wifi!");
          switch (getDevicesStatus()) {
            case 0:
              // if in rest time, switch off again if nobody is active
              //powerDown();
              setState (ST_ACT_SLEEP);
              break;
            case -1:
              // error, we need to poll again and just do nothing for now
              break;
            default:
              bot.sendMessage(CHAT_ID, "nice to see you again", "Markdown");
              // connected users
              if (tm->tm_hour > 17 && tm->tm_hour < 5)
                sw_lamp(1);
              setState (ST_ACT);
              break;
          }
        }
      }
      break;
  }
  if (millis() - status_poll >= STATUS_POLL_CYCLE) {
      status_poll = millis();
      Log.printf("state %d\n", state);
      handleTemp();
      handleStatusUpdate(1);
  }
  if (millis() - alarm_to >= 1000) {
    alarm_to = millis();
  }
}
