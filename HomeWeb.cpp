
#include "HomeWeb.h"

static char temp[1024];
const char script_js[] PROGMEM = R"=====(
function processFormData() {
    document.forms["action"].submit();
}
    
function btn_click(btn, q) {
    var val = 0;
    var sw;
    if (q == "ir") {
      val = btn.id.split('.')[1];
      sw = btn.id.split('.')[0];
    }
    else {
      var obj = JSON.parse(document.getElementById("serverData").innerHTML);
      for (i = 0; i < obj.switch.length; i++) {
          if (obj.switch[i].id == btn.id) {
              if (obj.switch[i].val)
                  obj.switch[i].val = 0;
              else
                  obj.switch[i].val = 1;
              val = obj.switch[i].val;
              sw = btn.id;
          }
        }
        document.getElementById("serverData").innerHTML = JSON.stringify(obj);
    }
    document.getElementById('query').value = q;
    document.getElementById('sw').value = sw;
    document.getElementById('on').value = val;
    document.forms["action"].submit();
    displayData();
}

function displayData() {
  var obj = JSON.parse(document.getElementById("serverData").innerHTML);
  document.getElementById("footer").innerHTML = 
  "| <a href=\"refresh\"> Refresh </a> | " + obj.time + "  | Alarms " + obj.alarms;
  for (i = 0; i < obj.sensor.length; i++) {
      document.getElementById(obj.sensor[i].id).innerHTML = obj.sensor[i].val;
  }
  for (i = 0; i < obj.switch.length; i++) {
    if (obj.switch[i].val == 1)
      document.getElementById(obj.switch[i].id).style.background = "#ffeb3b";
  else
      document.getElementById(obj.switch[i].id).style.background = "#ece086";
  }
}

function show_room(id) {
	document.getElementById("wohnzimmer").style.display = "none";
	document.getElementById("kueche").style.display = "none";
	document.getElementById("media").style.display = "none";
	document.getElementById(id).style.display = "block";
        w3_close();
}
function w3_open() {
	document.getElementById("main").style.marginLeft = "140px";
	document.getElementById("mySidenav").style.width = "140px";
	document.getElementById("mySidenav").style.display = "block";
	document.getElementById("openNav").style.display = 'none';
}
function w3_close() {
	document.getElementById("main").style.marginLeft = "0";
	document.getElementById("mySidenav").style.display = "none";
	document.getElementById("openNav").style.display = "inline-block";
}
)=====";

const char web_header [] PROGMEM = R"=====(<!DOCTYPE html>
<html>
<title>Home</title>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css">
<link rel="stylesheet" href="http://www.w3schools.com/lib/w3.css">
<style>
  .data { font-size:8px;color:#333333;margin-right:10px; }
  .btn { font-size:15px;color:#000;margin-right:10px; }
</style>
)=====";

const char web_nav[] PROGMEM = R"=====(
	<nav class="w3-sidenav w3-collapse w3-white w3-card-2 w3-animate-left" style="width:150px;" id="mySidenav">
  <a href="javascript:void(0)" onclick="w3_close()" 
  class="w3-closenav w3-large w3-hide-large w3-teal"><i class="fa fa-home btn"></i>Rooms &times;</a>
  <a href="#" onclick="show_room('kueche')">K&uuml;che</a>
  <a href="#" onclick="show_room('wohnzimmer')">Wohnzimmer</a>
  <a href="#" onclick="show_room('media')">Media</a>
  <a href="sensors">Sensoren</a>
  <a href="config"><i class="fa fa-cog btn"></i>Config</a>
</nav>)=====";

const char web_main[] PROGMEM = R"=====(
<div class="w3-main" style="margin-left:150px">
<div id="main">
<header class="w3-container w3-teal">
<span class="w3-opennav w3-xlarge w3-hide-large" onclick="w3_open()">&#9776; Home </span>
<a style="margin-left:34px" href="dis">
<img src="http://unicodepowersymbol.com/wp-content/uploads/2014/01/Red-Power-Symbol.png"
width="24px" heigth="24px"></a></header>
)=====";

int HomeWeb::init(ESP8266WebServer* server) {
	this->_server = server;
	
	/*server->on ( "/reset", []() {
		ESP.restart();
	} );
	*/
	server->begin();
	
	return 0;
}

/**
 * label Text on the button
 * id HTML tag id
 * url URL or suburl to call incl. parameters. It starts with sw=
 */
void HomeWeb::addButton(String& content, const char* label, int val, const char* url)
{
	const char* color[3] = { "w3-khaki", "w3-yellow", "btn" };
  const char* q[3] = { "ir", "", "sw" };

	content += "<div class=\"w3-row\"><div class=\"w3-col w3-container\">";
	content += "<p class=\"w3-text-black\">\n\
	<a class=\"w3-btn w3-round-large ";
	content += color[val];
	content += "\" id=\"";
	content += url;
	content += "\"";
#if 0
	content += "href='?sw=";
	content += url;
	content += String(val ? 0 : 1);
#else
  content += " onclick=\"btn_click(this, '";
	content += q[val];
  content += "');";
#endif
	content += "\">";
	content += label;
	content += "</a></p>";
	/* end of row and of container */
	content += "</div></div>";
}

void HomeWeb::handleConfig(int presence_det, int telegram, String& content)
{
	content = String(web_header);
	content += String(R"=====(<body style="max-width:600px"><div id="serverData" font-size="-2">)=====");
	snprintf(temp, sizeof(temp), "<div id=\"config\" class=\"w3-container\">");
	content += temp;
	content += R"=====(<form class="w3-container" action="cfg" method="POST">
		<p>Password <input class="w3-text" name="passwd" type="text"></p>)=====";
	// presence detection option
	content += R"=====(<p><input class="w3-checkbox" name="presence" type="checkbox")=====";
	if (presence_det == 1)
		content += " checked";
	content += ">Presence Detection</p>";
	// telegram interface option
	content += R"=====(<p><input class="w3-checkbox" name="telegram" type="checkbox")=====";
	if (telegram == 1)
		content += " checked";
	content += ">Telegram Interface</p>";
	content += R"=====(<p align="right"><input type="submit" value="Apply" class="w3-btn w3-round-large w3-small w3-blue-grey" type="button"></p>
		</form>)=====";
	content +=  "</div>";
	content += R"====(<footer class="w3-container w3-theme w3-margin-top w3-teal">
		| <a href="refresh"> Refresh </a> |
		</footer></div></div></body></html>)====";
}

void HomeWeb::startPage(String& content)
{
	startPage(content, "");
}

void HomeWeb::startPage(String& content, String data)
{
	content.reserve(0x2000);
	content = String(web_header);
	content += String(R"=====(
	<script type="text/javascript">)=====");
	content += String(script_js);
	content += "</script>";
	content += String(R"=====(<body style="max-width:600px"><div id="serverData" class="data">)=====");
	content += data;
	content += String(R"=====(</div>
	<script type="text/javascript">
	if (typeof(EventSource) != "undefined") {
	//create an object, passing it the name and location of the server side script
	var eSource = new EventSource("status");
	//detect message receipt
	eSource.onmessage = function(event) {
		//write the received data to the page
		document.getElementById("serverData").innerHTML = event.data;
		displayData();
	};
  } else {
      document.getElementById("serverData").innerHTML = "Whoops! Your browser doesn't receive server-sent events.";
  }</script> )=====");
	content += String(web_nav);
	content += String(web_main);
}

void HomeWeb::endPage(String& content, int ow_intPeding)
{
	content += String(R"=====(
	<iframe style="display: none;" src="" name="formtarget">
	<p>iframes are not supported by your browser.</p>
	</iframe>
	<form id="action" action="action" class="w3-container" target="formtarget" method="POST">
	<p align="left">
	<input class="w3-text" name="query" id="query" type="text">
	<input class="w3-text" name="sw" id="sw" type="text">
	<input class="w3-text" name="on" id="on" type="text">
	<input type="submit" value="Apply" class="w3-btn w3-round-large w3-small w3-blue-grey" type="button" onclick="processFormData();">
	</p>
	</form>
	<div id="status" class="data"></div>
	<footer class="w3-container w3-theme w3-margin-top w3-teal" id="footer">
	| <a href="refresh"> Refresh </a> |  | Alarms 
	</footer></div></div></div>
	<script type="text/javascript" id="myScript">
		displayData();
		document.getElementById("status").innerHTML = "loading complete";
	</script> 
	)=====");
	content += "</body></html>";
}

void HomeWeb::startSection(String& content, const char* title)
{
	content += "<div id=\"";
	content += title;
	content += "\" class=\"w3-container\">\n";
}

void HomeWeb::endSection(String& content)
{
	content += "</div>\n";
}
