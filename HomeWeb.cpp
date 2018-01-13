#include "HomeWeb.h"
#include "HomeConfig.h"
#include "version.h"

const char web_header [] PROGMEM = R"=====(<!DOCTYPE html>
<html><title>Home</title>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css">
<link rel="stylesheet" href="style.css" type="text/css">
<script type="text/javascript" src="script.js"></script>
<body style="max-width:600px">)=====";

const char web_nav[] PROGMEM = R"=====(
<div class="sidebar teal large" style="display:none" id="mySidenav">
  <button onclick="w3_close()" class="xlarge button bar-item">Close &times;</button>
  <a class="button bar-item" href="index.html" id="home"><i class="fa fa-home"></i>&nbsp; Home</a>
  <a class="button bar-item" href="sensors" id="sensors"><i class="fa fa-low-vision fa-fw"></i>&nbsp; Sensoren</a>
  <a class="button bar-item" href="wifi" id="wifi"><i class="fa fa-wifi fa-fw"></i>&nbsp; WLAN</a>
  <a class="button bar-item" href="config" id="config"><i class="fa fa-cog"></i>&nbsp; Config</a>
  <a class="button bar-item" onclick="btn_click(this, 'dis');"><i class="fa fa-power-off fa-fw"></i>&nbsp;
 Power</a>
</div>
<div class="teal">
  <button class="button teal xlarge" onclick="w3_open()">&#9776;</button><span class="xlarge">Home</span>
</div>
<div id="main">)=====";

const char web_form[] PROGMEM = R"=====(
<iframe style="display: none;" src="" name="formtarget"></iframe>
<form id="action" style="display:none" action="action" class="container" target="formtarget" method="POST">
<input name="query" id="query" type="text" size='6'>
<input name="sw" id="sw" type="text" size='2'>
<input name="on" id="on" type="text" size='10'>
<input type="submit" value="Apply" class="btn grey" type="button" onclick="processFormData();">
</form>
<div id="serverData" class="data" style="display:none"></div>
<div id="status" class="data" style="display: none"></div>)=====";
const char event_script[] PROGMEM = R"=====(
<script type="text/javascript">
if (typeof(EventSource) != "undefined") {
	var eSource = new EventSource("status");
	eSource.onmessage = function(event) {
	document.getElementById("serverData").innerHTML = event.data;
	displayData(JSON.parse(event.data));
	};
}
</script>)=====";

static const char web_foot[] PROGMEM = R"=====(
<footer class="footer">V1.1.1.0 | <a href="#" onclick="debug();">
<i class="fa fa-bug"></i> Debug </a> |
<span id="info"></span></footer></div></body></html>)=====";

void HomeWeb::setup(ESP8266WebServer *server)
{
	_server = server;
}

/**
 * label Text on the button
 * id HTML tag id
 * url URL or suburl to call incl. parameters. It starts with sw=
 */
void HomeWeb::addButton(String& content, const char* label, int val, const char* url)
{
#ifdef DYN_INDEX
	const char* color[3] = { "w3-khaki", "w3-yellow", "btn" };
	const char* q[3] = { "ir", "", "sw" };

	content += F("<div class=\"w3-row\"><div class=\"w3-col w3-container\">");
	content += "<p class=\"w3-text-black\">\n\
	<a class=\"w3-btn w3-round-large ";
	content += color[val];
	content += "\" id=\"";
	content += url;
	content += "\"";
	content += " onclick=\"btn_click(this, '";
	content += q[val];
	content += "');";
	content += "\">";
	content += label;
	content += "</a></p>";
	/* end of row and of container */
	content += "</div></div>";
#endif
}

void HomeWeb::handleConfig(HomeConfig &cfg)
{
	static const char frame[] PROGMEM = R"==(
	<script>document.getElementById("config").style.display = \"none\";</script>
	<iframe style="display: none;" src="" name="formtarget"></iframe>
	<form class="container" action="cfg" method="POST" target="_self">
	<p>SSID <input name="ssid" type="text" value=")==";
	static const char endform[] PROGMEM = R"=====(>Telegram Interface</p><p>Syslog IP <input name="syslog" type="text"></p>
	<p><input name="store" type="checkbox">Store Config</p>
	<p align="right"><input type="submit" value="Apply" class="btn grey" type="button"></p>
	</form>)=====";

	String content = String(cfg.ssid) + "\"></p>";
	content += F("<p>Password <input  name=\"passwd\" type=\"password\"></p>");
	// presence detection option
	content += F("<p><input name=\"presence\" type=\"checkbox\"");
	if (cfg.presence == 1)
		content += F(" checked");
	content += F(">Presence Detection</p>");
	// telegram interface option
	content += F("<p><input name=\"telegram\" type=\"checkbox\"");
	if (cfg.telegram == 1)
		content += F(" checked");

	_sendHeader(false, false, content.length() + strlen_P(frame) + strlen_P(endform));
	_server->sendContent_P(frame);
	_server->sendContent(content);
	_server->sendContent_P(endform);
	_sendFooter(false, false);
}

void HomeWeb::sendSensorsPage(String& content)
{
	static const char web_form[] PROGMEM = R"=====(
	<script>document.getElementById("sensors").style.display="none";</script>	
	<form id="action" action="sensors" class="container" target="_self" method="POST">
	<input name="query" id="query" type="text" size='6' value="cmd" style="display:none">
	<input name="sw" id="sw" type="text" size='2'>
	<input name="on" id="on" type="text" size='10'>
	<input type="submit" value="Apply" class="btn grey" type="button" onclick="processFormData();">
	</form>
	<div class="container">
		<a href="#" class="btn grey" onclick="btn_click(this, 'refresh');"> Refresh </a> 
	</div>)=====";

	_sendHeader(false, false, content.length() + strlen_P (web_form));
	_server->sendContent(content);
	_server->sendContent_P(web_form);
	_sendFooter(false, false);
}

void HomeWeb::_sendHeader(const bool form, const bool active, int size)
{
	size += strlen_P(web_header) + strlen_P(web_nav) + strlen_P(web_foot);
	if (form)
		size += strlen_P(web_form);
	if (active)
		size += strlen_P(event_script);
	_server->setContentLength(size);
	_server->send(200, F("text/html"), "");
	_server->sendContent_P(web_header);
	_server->sendContent_P(web_nav);
}

void HomeWeb::_sendFooter(const bool form, const bool active)
{
	if (form)
		_server->sendContent_P(web_form);
	if (active)
		_server->sendContent_P(event_script);
	_server->sendContent_P(web_foot);
}

void HomeWeb::sendPage_P(const String &name, PGM_P content,
	bool form, bool active)
{
	String activeName;

	activeName = F("<script>document.getElementById(\"");
	activeName += name;
	activeName += F("\").style.display=\"none\";</script>");
	_sendHeader(form, active, strlen_P(content) + activeName.length());
	_server->sendContent(activeName);
	_server->sendContent_P(content);
	_sendFooter(form, active);
}

void HomeWeb::sendPage(const String &name, const String& content,
	bool form, bool active)
{
	String activeName;

	activeName = F("<script>document.getElementById(\"");
	activeName += name;
	activeName += F("\").style.display=\"none\";</script>");
	_sendHeader(form, active, activeName.length() + content.length());
	_server->sendContent(activeName + content);
	_sendFooter(form, active);
}

void HomeWeb::startSection(String& content, const char* title)
{
	content += "<div id=\"";
	content += title;
	content += F("\" class=\"container\">\n");
}

void HomeWeb::endSection(String& content)
{
	content += "</div>\n";
}
