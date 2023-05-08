#include <WiFi.h>
#include "Arduino.h"

//Replace with your network credentials
// IP Address for Phone
//const char* ssid = "HUAWEI nova 3i";
//const char* password = "8b6930096d5a";

// IP Address for Home
//const char* ssid = "SINGTEL-4B7C";
//const char* password = "oochujezac";
const char* ssid = "SINGTEL-3BA0";
const char* password = "iesivawoov";

// Set your Static IP address
// IP Address for Home
IPAddress local_IP(192, 168, 1, 184);
// Set your Gateway IP address
//Home with Port Forwarding
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);

// IP Address for Phone
//IPAddress local_IP(192, 168, 43, 184);
//Phone for test but cannot do port forwarding
//IPAddress gateway(192, 168, 43, 1);
//IPAddress subnet(255, 255, 255, 0);

void wifisetup(){
  // Wi-Fi connection
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(ssid);
  
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(WiFi.localIP());
}
