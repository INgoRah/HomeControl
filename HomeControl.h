const char web_script_js [] PROGMEM = R"=====(
var json_data = null;
var dbg=0;
function processFormData() {
    document.forms["action"].submit();
}
    
function debug() {
    if (dbg == 0) {
        show = "inline-block";
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("status").innerHTML = "<pre>"+this.responseText+"</pre>";
            }
        };
        xhttp.open("GET", "log", true);
        xhttp.send(); 
    }
    else
        show = "none";
    dbg = !dbg;
    document.getElementById("status").style.display = show;
    document.getElementById("serverData").style.display = show;
    document.getElementById("action").style.display = show;
}

function btn_click(btn, q) {
    var val = 0;
    var sw;
    
    document.getElementById('query').value = q;
    if (q == "ir") {
      val = btn.id.split('.')[1];
      sw = btn.id.split('.')[0];
    } else
        sw = btn.id;
    if (q == "cmd") {
      for (i = 0; i< 5; i++)
          if (document.getElementById(i) != null)
            document.getElementById(i).style.background = "#a3a3a3";
      document.getElementById(sw).style.background = "#ffaa33";
      document.getElementById('sw').value = sw;
      return;
    }
    if (q == "sw") {
      var obj = json_data;
      for (i = 0; i < obj.switch.length; i++) {
          if (obj.switch[i].id == btn.id) {
              if (obj.switch[i].val)
                  obj.switch[i].val = 0;
              else
                  obj.switch[i].val = 1;
              val = obj.switch[i].val;
          }
      }
      displayData(json_data);
    }
    document.getElementById('sw').value = sw;
    document.getElementById('on').value = val;
    document.forms["action"].submit();
}

function displayData(json) {
  document.getElementById("serverData").innerHTML = JSON.stringify(json);
  
  if (json.log != null) {
	if (document.getElementById("status") != null)
                document.getElementById("status").innerHTML =
			 "<pre>"+document.getElementById("status").innerHTML + json.log +"</pre>";
  }
  if (document.getElementById("info") != null)
    document.getElementById("info").innerHTML = 
      json.time + "  | Alarms " + json.alarms;
  if (json.sensor == null)
      return;
  for (i = 0; i < json.sensor.length; i++) {
    if (document.getElementById(json.sensor[i].id) == null)
        continue;
    document.getElementById(json.sensor[i].id).style.display = "inline";
    document.getElementById(json.sensor[i].id).innerHTML = json.sensor[i].val;
  }
  if (json.switch == null)
      return;
  for (i = 0; i < json.switch.length; i++) {
    if (document.getElementById(json.switch[i].id) == null)
        continue;
    if (json.switch[i].id == "2.2")
        document.getElementById("media").style.display = "block";
    document.getElementById(json.switch[i].id).style.display = "inline";
    if (json.switch[i].val == 1)
      document.getElementById(json.switch[i].id).style.background = "#ffaa33";
  else
      document.getElementById(json.switch[i].id).style.background = "#ece086";
  }
  json_data = json;
}

function w3_open() {
	document.getElementById("mySidenav").style.display = "block";
	document.getElementById("main").style.marginLeft = "140px";
}
function w3_close() {
	document.getElementById("mySidenav").style.display = "none";
	document.getElementById("main").style.marginLeft = "0";
}
)=====";

const char web_style[] PROGMEM = R"=====(html{box-sizing:border-box}*,*:before,*:after{box-sizing:inherit}
html{-ms-text-size-adjust:100%;-webkit-text-size-adjust:100%}
body{margin:0}
html,body{font-family:Verdana,sans-serif;font-size:15px;line-height:1.5}
html{overflow-x:hidden}
p,footer,header,main,menu,nav{display:block}
footer{font-size:10px}
p{padding:6px 0px}
a{background-color:transparent;-webkit-text-decoration-skip:jsonects}
a{color:inherit;font-weight:inherit}
.btn, .button, a{text-decoration:none!important}
a:active,a:hover{outline-width:0}
.header,footer{padding:0.02em 24px;margin-top:2px;color:#fff!important;background-color:#009688!important}
.header {content:"";display:table;clear:both}
.footer{margin-top:16px}
.xlarge{vertical-align:middle;font-size:22px!important}
.data { font-size:10px;color:#333333;margin-right:10px;padding:0px 24px}
.btn, .button, .check {border:none;display:inline-block;outline: 0;vertical-align: middle;overflow: hidden;text-align:center;cursor:pointer;white-space:nowrap}
.btn {margin-left: 10px;margin-right: 10px;font-size: 14px;color: #333333;border-radius: 8px !important;padding: 8px 20px;background-color: #f0e68c}
.check {background-color:#a3a3a3;margin-left: 2px;margin-right: 4px;font-size: 12px;color: #333333;border-radius: 6px !important;padding: 4px 4px}
.button {padding:6px 16px;background-color:#000;white-space:nowrap}
.button {color:#000;background-color:#f1f1f1}
.button:hover{color:#000!important;background-color:#ccc!important}
.bar, .bar-item{padding:8px 16px;float:left;background-color:inherit;color:inherit;width:auto;border:none;outline:none;display:block}
.bar-item{width:100%;display:block;text-align:left;white-space:normal}
.bar-item-hover, .button-hover,.btn:hover {box-shadow:0 8px 16px 0 rgba(0,0,0,0.2),0 6px 20px 0 rgba(0,0,0,0.19)}
.khaki, .hover-khaki:hover{background-color: #f0e68c !important;display: inline-block}
.teal,.hover-teal:hover{color:#fff!important;background-color:#009688!important}
.grey,.hover-grey:hover{background-color:#a3a3a3!important}
.temp{color:#f44336!important;font-size:20px;margin-left:40px}
.container, .panel{margin-top:10px}
.container {padding:12px 24px}
.line {padding:2px 24px}
.panel{padding:0.01em 16px}
.panel{margin-bottom:10px}
.sidebar{height:100%;width:140px;background-color:#fff;position:fixed!important;z-index:1;overflow:auto}
div.container:nth-of-type(even) {background: #e0e0e0})=====";
