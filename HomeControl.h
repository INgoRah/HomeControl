
const char web_script_js [] PROGMEM = R"=====(
function btn_click(id) {
}
  
function show_room(id) {
    document.getElementById("wohnzimmer").style.display = "none";
    document.getElementById("kueche").style.display = "none";
    document.getElementById("media").style.display = "none";
    //document.getElementById("schlaf").style.display = "none";
    document.getElementById(id).style.display = "block";
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
<script id="myScript" src="script.js"></script>
<style>
	.btn {
	font-size:20px;color:gray;margin-right:10px;
	}
</style>
<body style="max-width:600px">)=====";

const char web_nav [] PROGMEM = R"=====(
<nav class="w3-sidenav w3-collapse w3-white w3-card-2 w3-animate-left" style="width:140px;" id="mySidenav">
  <a href="javascript:void(0)" onclick="w3_close()" 
  class="w3-closenav w3-large w3-hide-large w3-teal"><i class="fa fa-home btn"></i>Rooms &times;</a>
  <a href="#" onclick="show_room('kueche')">K&uuml;che</a>
  <a href="#" onclick="show_room('wohnzimmer')">Wohnzimmer</a>
  <a href="#" onclick="show_room('media')">Media</a>
  <a href="sensors">Sensoren</a>
  <a href="config"><i class="fa fa-cog btn"></i>Config</a>
</nav>)=====";

const char web_main [] PROGMEM = R"=====(
<div id="main">
<div class="w3-main" style="margin-left:150px">
<header class="w3-container w3-teal">
<span class="w3-opennav w3-xlarge w3-hide-large" onclick="w3_open()">&#9776; Home </span>
<a style="margin-left:34px" href="dis">
<img src="http://unicodepowersymbol.com/wp-content/uploads/2014/01/Red-Power-Symbol.png"
width="24px" heigth="24px"></a></header>
)=====";

#define MYFOOTER "<footer class=\"w3-container w3-theme w3-margin-top w3-teal\">\
| <a href=\"refresh\"> Refresh </a> | \
</footer></div></div>\
</body></html>"
