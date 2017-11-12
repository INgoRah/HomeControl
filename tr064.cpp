/*
  tr064.h - Library for communicating via TR-064 protocol
  (e.g. Fritz!Box)
  A descriptor of the protocol can be found here: https://avm.de/fileadmin/user_upload/Global/Service/Schnittstellen/AVM_TR-064_first_steps.pdf
  The latest Version of this library can be found here: http://github.com/Aypac
  Created by René Vollmer, November 2016
*/
#include <ESP8266HTTPClient.h>
#include "tr064.h"
#define STATIC_SERVICE_LIST

#define arr_len(x)  (sizeof(x) / sizeof(*x))
#define _requestStart "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
#define DETECT_PAGE "/tr64desc.xml"

//Do not construct this unless you have a working connection to the device!
TR064::TR064(int port, String ip, String user, String pass)
{
  _port = port;
  _ip = ip;
  _user = user;
  _pass = pass;
  _init = 0;
#ifdef STATIC_SERVICE_LIST
  /* got from http://192.168.178.1:49000/tr64desc.xml
  <serviceType>urn:dslforum-org:service:WLANConfiguration:1</serviceType>\
  ...
  <controlURL>/upnp/control/wlanconfig1</controlURL>\
  */
  String s[][2] = {
      { "WLANConfiguration:1", "/upnp/control/wlanconfig1" },
      { "WLANConfiguration:2", "/upnp/control/wlanconfig2" },
      { "WLANConfiguration:3", "/upnp/control/wlanconfig3" },
      { "Hosts:1", "/upnp/control/hosts" },
      { "LANEthernetInterfaceConfig:1", "/upnp/control/lanethernetifcfg" },
      { "LANHostConfigManagement:1", "/upnp/control/lanhostconfigmgm" },
      { "WANIPConnection:1", "/upnp/control/wanipconnection1" }
   };
   int i;
   for (i = 0; i < 6; ++i) {
      _services[i][0] = s[i][0];
      _services[i][1] = s[i][1];
    }
#endif
}

//DONT FORGET TO INIT!
void TR064::init() {
  if (initServiceURLs() == -1)
    return;
  //Get the initial nonce and the realm
  initNonce();
    //Now we have everything to generate out hased secret.
  //Serial.println("Your secret is is: " + _user + ":" + _realm + ":" + _pass);
  _secretH = md5String(_user + ":" + _realm + ":" + _pass);
  //Serial.println("TR64: Your secret is hashed: " + _secretH);
  _init = 1;
}

//Fetches a list of all services and the associated urls
// memory hungry and might crash
// surely crashed on my FritzBox 7490
int TR064::initServiceURLs() {
#ifndef STATIC_SERVICE_LIST
  String inStr;
  int CountChar=7; //length of word "service"
  int i = 0;

  inStr = httpRequest(DETECT_PAGE, "", "");
  if (inStr.length() == 0)
    return -1;
  
  while (inStr.indexOf("<service>") > 0 || inStr.indexOf("</service>") > 0) {
    int indexStart=inStr.indexOf("<service>");
    int indexStop= inStr.indexOf("</service>");

    String serviceXML = inStr.substring(indexStart+CountChar+2, indexStop);
    String servicename = xmlTakeParam(serviceXML, "serviceType");
    servicename.replace("urn:dslforum-org:service:", "");
    String controlurl = xmlTakeParam(serviceXML, "controlURL");
    _services[i][0] = servicename;
    _services[i][1] = controlurl;
    //Serial.flush();
    Serial.println(servicename + " @ " + controlurl);
    inStr = inStr.substring(indexStop+CountChar+3);
    if (++i > MAX_SERVICES)
      return -1;
  }
#endif
  return 0;
}

//Fetches the initial nonce and the realm
int TR064::initNonce() {
    //Serial.print("Geting the initial nonce and realm\n");
    String a[][2] = {{"NewAssociatedDeviceIndex", "1"}};
    if (action("WLANConfiguration:1", "GetGenericAssociatedDeviceInfo", a, 1).length() > 0)
      return 0;
    Serial.println ("TR64: Nonce get failed");
    return -1;
    //Serial.print("TR64: Got the initial nonce: " + _nonce + " and the realm: " + _realm + "\n");
}

//Returns the xml-header for authentification
String TR064::generateAuthXML() {
    String token;
    if (_nonce == "") { //If we do not have a nonce yet, we need to use a different header
       token="<s:Header><h:InitChallenge xmlns:h=\"http://soap-authentication.org/digest/2001/10/\" s:mustUnderstand=\"1\"><UserID>"+_user+"</UserID></h:InitChallenge ></s:Header>";
    } else { //Otherwise we produce an authorisation header
      token = generateAuthToken();
      token = "<s:Header><h:ClientAuth xmlns:h=\"http://soap-authentication.org/digest/2001/10/\" s:mustUnderstand=\"1\"><Nonce>" + _nonce + "</Nonce><Auth>" + token + "</Auth><UserID>"+_user+"</UserID><Realm>"+_realm+"</Realm></h:ClientAuth></s:Header>";
    }
    return token;
}

//Returns the authentification token based on the hashed secret and the last nonce.
String TR064::generateAuthToken() {
    String token = md5String(_secretH + ":" + _nonce);
    //Serial.print("The auth token is " + token + "\n");
    return token;
}


//This function will call an action on the service.
String TR064::action(String service, String act) {
    //Serial.println("action_2");
    String p[][2] = {{}};
    return action(service, act, p, 0);
}

//This function will call an action on the service.
//With params you set the arguments for the action
//e.g. String params[][2] = {{ "arg1", "value1" }, { "arg2", "value2" }};
String TR064::action(String service, String act, String params[][2], int nParam) {
    //Serial.println("action_1");
    //Generate the xml-envelop
    String xml = _requestStart;
    xml += generateAuthXML() + "<s:Body><u:"+act+" xmlns:u='urn:dslforum-org:service:" + service + "'>";
    //add request-parameters to xml
    if (nParam > 0) {
        for (int i=0;i<nParam;++i)
          if (params[i][0] != "")
                xml += "<"+params[i][0]+">"+params[i][1]+"</"+params[i][0]+">";
    }
    //close the envelop
    xml += "</u:" + act + "></s:Body></s:Envelope>";
    //The SOAPACTION-header is in the format service#action
    String soapaction = "urn:dslforum-org:service:" + service+"#"+act;

    //Send the http-Request
    String xmlR = httpRequest(findServiceURL(service), xml, soapaction);
    //Extract the Nonce for the next action/authToken.
    if (xmlR != "") {
      if (xmlTakeParam(xmlR, "Nonce") != "") {
          _nonce = xmlTakeParam(xmlR, "Nonce");
      }
      if (_realm == "" && xmlTakeParam(xmlR, "Realm") != "") {
          _realm = xmlTakeParam(xmlR, "Realm");
      }
    } else {
      Serial.println (soapaction);
      Serial.println (xml);
    }
    return xmlR;
}

//This function will call an action on the service.
//With params you set the arguments for the action
//e.g. String params[][2] = {{ "arg1", "value1" }, { "arg2", "value2" }};
//Will also fill the array req with the values of the assiciated return variables of the request.
//e.g. String req[][2] = {{ "resp1", "" }, { "resp2", "" }};
//will be turned into req[][2] = {{ "resp1", "value1" }, { "resp2", "value2" }};
String TR064::action(String service, String act, String params[][2], int nParam, String (*req)[2], int nReq) {
    //Serial.println("action_3");
    String xmlR = action(service, act, params, nParam);
    String body = xmlTakeParam(xmlR, "s:Body");

    if (xmlR.length() > 0 && nReq > 0) {
        for (int i=0;i<nReq;++i) {
            if (req[i][0] != "") {
                req[i][1] = xmlTakeParam(body, req[i][0]);
            }
        }
    }
    return xmlR;
}

//Returns the (relative) url for a service
String TR064::findServiceURL(String service) {
    uint32_t i;
    
    for (i = 0; i < arr_len(_services); ++i) {
      if (_services[i][0] == service)
              return _services[i][1];
    }
    return ""; //Service not found error! TODO: Proper error-handling?
}

//Puts a http-Request to the given url (relative to _ip on _port)
// - if specified POSTs xml and adds soapaction as header field.
// - otherwise just GETs the url
String TR064::httpRequest(String url, String xml, String soapaction) {
    HTTPClient http;
    int httpCode=0;
    String payload = "";

    http.begin(_ip, _port, url);
    if (soapaction != "") {
      http.addHeader("CONTENT-TYPE", "text/xml"); //; charset=\"utf-8\"
      http.addHeader("SOAPACTION", soapaction);
    }
    //http.setAuthorization(fuser.c_str(), fpass.c_str());

    // start connection and send HTTP header
    if (xml != "")
      httpCode = http.POST(xml);
    else
      httpCode = http.GET();
    // httpCode will be negative on error
    if (httpCode == HTTP_CODE_OK) {
        // HTTP header has been send and Server response header has been handled
        // file found at server
        payload = http.getString();
         
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      Serial.println (url);
      //TODO: _nonce="";
    }

    //Serial.println("\n\n\n"+payload+"\n\n\n");
    http.end();
    return payload;
}

/*
 * FritzBox API
 */
 
/** 
 *  Get the number of devices that were connected to the WIFI lastly
 *  (some of them might not be online anymore, you need to check them individually!)
 *  return (int)
 */
int TR064::getDeviceCount() {
  String params[][2] = {{}};
  String req[][2] = {{"NewHostNumberOfEntries", ""}};
  String ret;

  if (_init == 0)
    return -1;
  ret = action("Hosts:1", "GetHostNumberOfEntries",
      params, 0, req, 1);
  if (ret.length() > 0)
    return (req[0][1]).toInt();
  else
    return -1;
}

int TR064::getWifiDeviceStatus(int numDev, String* ip, String* mac, int* active) {
  String req[][2] = {{"NewAssociatedDeviceAuthState", ""}, {"NewAssociatedDeviceMACAddress", ""}, {"NewAssociatedDeviceIPAddress", ""}};
  String params[][2] = {{"NewAssociatedDeviceIndex", String(numDev)}};
  String ret;
  
  if (_init == 0)
    return -1;
  ret = action("WLANConfiguration:1", "GetGenericAssociatedDeviceInfo", params, 1, req, 3);
  if (ret.length() == 0)
    return 0;

  *active = (req[0][1]).toInt();
  if (mac != NULL)
    *mac = req[1][1];
  if (ip != NULL)
    *ip = req[2][1];
  
  return 1;
}

int TR064::getDeviceStatus(int numDev, String* ip, String* name, int* active) {
  String req[][2] = {{"NewActive", ""}, {"NewHostName", ""}, {"NewIPAddress", ""}};
  String params[][2] = {{"NewIndex", String(numDev)}};
  String ret;
  
  if (_init == 0)
    return -1;
  ret = action("Hosts:1", "GetGenericHostEntry", params, 1, req, 3);
  if (ret.length() == 0)
    return 0;

  *active = (req[0][1]).toInt();
  if (name != NULL)
    *name = req[1][1];
  if (ip != NULL)
    *ip = req[2][1];
  
  return 1;
}

/** 
 *  Print the status of all devices that were connected to the WIFI lastly
 * (some of them might not be online anymore, also gets you the hostnames and macs)
 * return nothing as of yet
 */
#include <ESP8266WiFi.h>

int TR064::getDevicesStatus(bool all) {
  int numDev, i;
  String ip, name;
  int active;
  int overall_active = 0;
  
  if (_init == 0)
    return -1;
  numDev = getDeviceCount();
  if (numDev == -1)
    return -1;

  // Query the mac and status of each device
  for (i = 0; i < numDev; ++i) {
    if (getDeviceStatus(i, &ip, &name, &active) == -1)
      return -1;
    if (ip == _ip)
      continue;
    Serial.printf("%d:\t", i);
    if (active == 1)
      Serial.print("*");
    else
      Serial.print(" ");
    Serial.print(ip + " [" + name + "]");
    if (ip == WiFi.localIP().toString())
      Serial.print (" // me!");
    else if (active) {
      overall_active++;
      if (all == false) {
        Serial.println ();
        return 1;
      }
    }
    Serial.println ();
  }

  return overall_active;
}

/** 
 *  Get the status of one very specific device. May contain less information as the same option for WIFI.
 * return nothing, but fills the array r
 */
String TR064::getDeviceName(String mac) {
    //Ask for one specific device
    String params[][2] = {{"NewMACAddress", mac}};
    String req[][2] = {/*{"NewIPAddress", ""}, {"NewActive", ""}, */ {"NewHostName", ""}};
    
    action("Hosts:1", "GetSpecificHostEntry", params, 1, req, 1);

    return req[0][1];
}

//----------------------------
//----- Helper-functions -----
//----------------------------

String TR064::md5String(String text){
  int i;
  byte bbuff[16];
  String hash = "";
  MD5Builder nonce_md5;

  nonce_md5.begin();
  nonce_md5.add(text); 
  nonce_md5.calculate(); 
  nonce_md5.getBytes(bbuff);
  for (i = 0; i < 16; i++) 
    hash += byte2hex(bbuff[i]);

  return hash;   
}

String TR064::byte2hex(byte number){
  String Hstring = String(number, HEX);
  
  if (number < 16)
    Hstring = "0" + Hstring;

  return Hstring;
}

//Extract the content of an XML tag - case-insensitive
//Not recommend to use directly/as default, since XML is case-sensitive by definition
//Is made to be used as backup.
String TR064::xmlTakeParami(String inStr,String needParam) {
    //TODO: Give warning?
  needParam.toLowerCase();
  String instr = inStr;
  instr.toLowerCase();
  int indexStart = instr.indexOf("<"+needParam+">");
  int indexStop = instr.indexOf("</"+needParam+">");  
  if (indexStart > 0 || indexStop > 0) {
     int CountChar=needParam.length();
     return inStr.substring(indexStart+CountChar+2, indexStop);
  }
    //TODO: Proper error-handling?
  return "";
}

//Extract the content of an XML tag
//If you cannot find it case-sensitive, look case insensitive
String TR064::xmlTakeParam(String inStr, String needParam) {
   int indexStart = inStr.indexOf("<"+needParam+">");
   int indexStop = inStr.indexOf("</"+needParam+">");  
   if (indexStart > 0 || indexStop > 0) {
     int CountChar=needParam.length();
     return inStr.substring(indexStart+CountChar+2, indexStop);
   }
   //As backup
   return xmlTakeParami(inStr, needParam);
}
