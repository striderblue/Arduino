#include <SPIFFS.h>
#include <ESPAsync_WiFiManager.h>
#include <ArduinoJson.h>

#define RESET_BTN 0
#define HTTP_PORT 80

AsyncWebServer  webServer(HTTP_PORT);
DNSServer       dnsServer;

String ssid = "ESP_" + String(ESP_getChipId(), HEX);
const char* password = "1234";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial); delay(200);

  pinMode(RESET_BTN, INPUT);

  Serial.print("\nStarting Async_AutoConnect_ESP32 on " + String(ARDUINO_BOARD));
  Serial.println(ESP_ASYNC_WIFIMANAGER_VERSION);

  ESPAsync_WiFiManager ESPAsync_WiFiManager(&webServer, &dnsServer, "Async_AutoConnect");
  // ESPAsync_WiFiManager.autoConnect("AutoConnectAP", "golf1234");
  ESPAsync_WiFiManager.startConfigPortal((const char *) ssid.c_str(), password);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected! Local IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(ESPAsync_WiFiManager.getStatus(WiFi.status()));
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if (digitalRead(RESET_BTN) == LOW) {
    ESP.restart();
  }
}
