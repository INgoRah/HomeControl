  /*
  tr064.h - Library for communicating via TR-064 protocol
  (e.g. Fritz!Box)
  A descriptor of the protocol can be found here: https://avm.de/fileadmin/user_upload/Global/Service/Schnittstellen/AVM_TR-064_first_steps.pdf
  The latest Version of this library can be found here: http://github.com/Aypac
  Created by René Vollmer, November 2016
*/


#ifndef tr064_h
#define tr064_h

//#define USE_SERIAL Serial
#include "Arduino.h"
#define MAX_SERVICES 7
class TR064
{
  public:
    TR064(int port, String ip, String user, String pass);
    void init();
    int getDeviceCount();
    int getHostDevicesStatus(bool all);
    int getDeviceStatus(int numDev, String* ip, String* name, int* active);
    int getWifiDeviceCount(int wlan);
    int getWifiDeviceStatus(int wlan, int numDev, String* ip, String* mac);
    int getWifiDevicesStatus(bool all);
    String getDeviceName(String mac);
  private:
    String action(String service, String act);
    String action(String service, String act, String params[][2], int nParam);
    String action(String service, String act, String params[][2], int nParam, String (*req)[2], int nReq);
    String action(String service, String url, String act, String params[][2], int nParam);

    String xmlTakeParam(String inStr,String needParam);
    String md5String(String s);
    String byte2hex(byte number);
    int initServiceURLs();
    int initNonce();
    String httpRequest(String url, String xml, String action);
    String generateAuthToken();
    String generateAuthXML();
    String findServiceURL(String service);
    String xmlTakeParami(String inStr,String needParam);
    String _ip;
    int _port;
    String _user;
    String _pass;
    String _realm; //To be requested from the router
    String _secretH; //to be generated
    String _nonce = "";
    String _services[MAX_SERVICES][2];
    int _init;
};

#endif
