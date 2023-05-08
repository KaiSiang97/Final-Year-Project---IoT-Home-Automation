#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>  // Downloaded online for Telegram
#include <ArduinoJson.h>           // Install from the Library

WiFiMulti wifiMulti;

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 10000;

// Initialize Telegram BOT
String BOTtoken = "5742840189:AAH_ASbOjhRqS-SMGpMf6Mxw2ipvHRgE1qA";  // Get from Botfather
String CHAT_ID = "927086240";  // Get from IDBot

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

void wifisetup(){
  Serial.begin(115200);
  delay(10);
  WiFi.mode(WIFI_STA);
  
  // Add list of wifi networks
  while(wifiMulti.run() != WL_CONNECTED)
  {
    Serial.println("WiFi connecting...");
    wifiMulti.addAP("SINGTEL-4B7C", "oochujezac");
    wifiMulti.addAP("SINGTEL-3BA0", "iesivawoov");
    wifiMulti.addAP("HUAWEI nova 3i", "8b6930096d5a");
    delay(1000);
  }
  

  // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
  Serial.println("Connecting Wifi...");
  if(wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println(WiFi.SSID());
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  }
}
