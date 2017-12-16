const char web_script_js [] PROGMEM = R"=====(
function processFormData() {
}
    
function btn_click(id, val) {
    var obj = JSON.parse(document.getElementById("serverData").innerHTML);
    for (i = 0; i < obj.switches; i++) {
        if (obj.switch[i].id == id.id) {
            console.log("changing " + id.id);
            if (obj.switch[i].val)
                obj.switch[i].val = 0;
            else
                obj.switch[i].val = 1;
            val = obj.switch[i].val;
        }
    }
    obj.alarms++;
    document.getElementById("serverData").innerHTML = JSON.stringify(obj);
    document.getElementById('query').value = "sw" + id.id;
    document.getElementById('val').value = val;
    // Create the iFrame used to send our data
    document.forms["action"].submit();
    displayData();
}

function displayData() {
  var obj = JSON.parse(document.getElementById("serverData").innerHTML);
  document.getElementById("footer").innerHTML = 
  "| <a href=\"refresh\"> Refresh </a> | " + obj.time + "  | Alarms " + obj.alarms;
  for (i = 0; i < obj.switches; i++) {
    if (obj.switch[i].val == 1)
      document.getElementById(obj.switch[i].id).style.background = "#ffeb3b";
  else
      document.getElementById(obj.switch[i].id).style.background = "#f0e68c";
  }
}

function update(data) {
   var status = JSON.parse(data);
   //document.getElementById("serverData").innerHTML = "<font size=-2>" + data + "</font>";
}

function updateData() {
    /*
 var xhttp = new XMLHttpRequest();
 xhttp.onreadystatechange = function() {
  if (this.readyState == 4 && this.status == 200) {
   console.log("updateData(): got Data:", this.responseText);
   update(this.responseText);
  }
 };
 xhttp.open("GET", "/status", true);
 xhttp.send();
 */
}

    function show_room(id) {
        document.getElementById("wohnzimmer").style.display = "none";
        document.getElementById("kueche").style.display = "none";
        document.getElementById("media").style.display = "none";
        document.getElementById(id).style.display = "block";
    }
    function w3_open() {
      //document.getElementById("main").style.marginLeft = "140px";
      //document.getElementById("mySidenav").style.width = "140px";
      document.getElementById("mySidenav").style.display = "block";
      document.getElementById("openNav").style.display = 'none';
    }
    function w3_close() {
      //document.getElementById("main").style.marginLeft = "0";
      document.getElementById("mySidenav").style.display = "none";
      document.getElementById("openNav").style.display = "inline-block";
    }
)=====";
