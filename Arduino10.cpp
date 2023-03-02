#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <AutoConnect.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

#define ROW_NUM    5
#define COL_NUM    4
#define BUZZER_PIN 15
#define GREEN_LED  17
#define RED_LED    16

// Parameters
unsigned long previousWaiting = millis();
long timeIntervalWaiting = 10000;
char tempToken[50];
char tempQueue[50];
String wifiLocalIP;
String softAP;

// LCD
int totalColumns = 16;
int totalRows = 2;
LiquidCrystal_I2C lcd (0x27, totalColumns, totalRows);
// LiquidCrystal_I2C lcd (0x3F, totalColumns, totalRows);

void scrollMessage(int row, String message, int delayTime, int totalColumns) {
  for (int i = 0; i < totalColumns; i++) {
    message = " " + message;
  }
  message = message + " ";
  for (int position = 0; position < message.length(); position++) {
    lcd.setCursor(0, row);
    lcd.print(message.substring(position, position + totalColumns));
    delay(delayTime);
  }
}

void printMessage(int row, String message, int totalColumns) {
  lcd.setCursor(0, row);
  lcd.print(message);
  Serial.print("Print Message: "); Serial.println(message);
}

// Keypad
char keys[ROW_NUM][COL_NUM] = {
  {'S', 'E', 'R', 'H'},
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM] = {26, 25, 33, 32, 4};
byte pin_cols[COL_NUM] = {27, 14, 12, 13};

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_cols, ROW_NUM, COL_NUM);

// MQTT Broker
const char *mqtt_broker   = "10.88.88.150";
const char *mqtt_username = "admin";
const char *mqtt_password = "nt@rec00";
const char *pub_topic     = "ntqueue/topic/core";
const int   mqtt_port     = 1883;

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// SPIFFs and AutoConnect
struct myConfig {
  char employee_id[64];
  int channel_slot;
  int service_type;
};

const char* filename = "/config.txt";
myConfig mycnf;

const char PAGE_ADD[] PROGMEM = R"(
  {
    "uri": "/add",
    "title": "Channel Settings",
    "menu": true,
    "element": [
      {
        "name": "valueEmployee",
        "type": "ACInput",
        "label": "Employee ID",
        "apply": "text"
      },
      {
        "name": "valueChannel",
        "type": "ACInput",
        "label": "Channel Slot",
        "apply": "number"
      },
      {
        "name": "valueService",
        "type": "ACInput",
        "label": "Service Number",
        "apply": "number"
      },
      {
        "name": "add",
        "type": "ACSubmit",
        "value": "ADD",
        "uri": "/results"
      }
    ]
  }
)";

const char PAGE_RESULTS[] PROGMEM = R"(
  {
    "uri": "/results",
    "title": "Channel Configuration",
    "menu": false,
    "element": [
      {
        "name": "results",
        "type": "ACText",
        "posterior": "br"
      },
      {
        "name": "resEmployee",
        "type": "ACText",
        "posterior": "br"
      },
      {
        "name": "resChannel",
        "type": "ACText",
        "posterior": "br"
      },
      {
        "name": "resService",
        "type": "ACText",
        "posterior": "br"
      }
    ]
  }
)";

AutoConnect portal;
AutoConnectAux page_add;
AutoConnectAux page_results;
AutoConnectConfig acConfig;

void loadConfiguration(const char *filename, myConfig &mycnf) {
  File file = SPIFFS.open(filename);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration"));
  }

  mycnf.channel_slot = doc["channel_slot"] | 0;
  mycnf.service_type = doc["service_type"] | 1;
  strlcpy(mycnf.employee_id, doc["employee_id"] | "112233445566", sizeof(mycnf.employee_id));
  file.close();
}

void saveConfiguration(const char *filename, const myConfig &mycnf) {
  SPIFFS.remove(filename);

  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  StaticJsonDocument<256> doc;
  doc["channel_slot"] = mycnf.channel_slot;
  doc["service_type"] = mycnf.service_type;
  doc["employee_id"] = mycnf.employee_id;
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file."));
  }
  file.close();
}

void printFile(const char *filename) {
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file."));
    return;
  }

  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();
  file.close();
}

String onAdd(AutoConnectAux& aux, PageArgument& args) {
  aux["valueEmployee"].as<AutoConnectInput>().value = "";
  aux["valueChannel"].as<AutoConnectInput>().value = "0";
  aux["valueService"].as<AutoConnectInput>().value = "0";
  return String();
}

String onResults(AutoConnectAux& aux, PageArgument& args) {
  String valueEmployee = args.arg("valueEmployee");
  int valueChannel = args.arg("valueChannel").toInt();
  int valueService = args.arg("valueService").toInt();

  aux["results"].as<AutoConnectText>().value = "Register Complete!!!";
  aux["resEmployee"].as<AutoConnectText>().value = "Employee ID: " + valueEmployee;
  aux["resChannel"].as<AutoConnectText>().value = "Channel Slot: " + String(valueChannel);
  aux["resService"].as<AutoConnectText>().value = "Service Type: " + String(valueService);

  strlcpy(mycnf.employee_id, valueEmployee.c_str(), sizeof(mycnf.employee_id));
  mycnf.channel_slot = valueChannel;
  mycnf.service_type = valueService;
  Serial.println(F("Saving configuration..."));
  saveConfiguration(filename, mycnf);

  String sub_topic1 = "ntqueue/topic/terminal/" + String(valueService);
  mqttClient.subscribe(sub_topic1.c_str());
  Serial.print(sub_topic1); Serial.println(" Subscribed!");
  String sub_topic2 = "ntqueue/topic/terminal/" + String(valueService) + "/" + String(valueChannel);
  mqttClient.subscribe(sub_topic2.c_str());
  Serial.print(sub_topic2); Serial.println(" Subscribed!");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Subscribed!");
  return String();
}

// MQTT
void requestData(String statusStr, String statusTxt) {
  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> rdDoc;
  DeserializationError error = deserializeJson(rdDoc, rd);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration."));
  }

  const int capacity = JSON_OBJECT_SIZE(8);
  StaticJsonDocument<capacity> doc;
  doc["action"] = statusStr;
  doc["status"] = statusTxt;
  doc["service_type"] = rdDoc["service_type"];
  doc["channel_slot"] = rdDoc["channel_slot"];
  doc["employee_id"] = rdDoc["employee_id"];
  rd.close();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  if (mqttClient.publish(pub_topic, buffer, n) == true) {
    Serial.println(F("Success sending message."));
  } else {
    Serial.println(F("Fail sending message."));
  }
  Serial.println();
}

void publishData(String statusStr, String statusTxt) {
  //Serial.printf("publishing data: %s, %d, %s, %s, %s\r\n", empId, channel_slot, statusStr, statusTxt);

  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> rdDoc;
  DeserializationError error = deserializeJson(rdDoc, rd);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration."));
  }
  Serial.print("Token: "); Serial.println(tempToken);
  const int capacity = JSON_OBJECT_SIZE(10);
  StaticJsonDocument<capacity> doc;
  doc["action"] = statusStr;
  doc["status"] = statusTxt;
  doc["service_type"] = rdDoc["service_type"];
  doc["channel_slot"] = rdDoc["channel_slot"];
  doc["employee_id"] = rdDoc["employee_id"];
  doc["token"] = tempToken;
  rd.close();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  if (mqttClient.publish(pub_topic, buffer, n) == true) {
    Serial.println(F("Success sending message."));
  } else {
    Serial.println(F("Fail sending message."));
  }
  Serial.println();
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (mqttClient.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println(F("MQTT Broker connected."));
    } else {
      Serial.print("failed with state: "); Serial.println(mqttClient.state());
      delay(2000);
    }
  }
  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> rdDoc;
  DeserializationError rdError = deserializeJson(rdDoc, rd);
  if (rdError) {
    Serial.println(F("Failed to read file"));
  }
  String defaultServiceType = rdDoc["service_type"];
  String defaultChannel = rdDoc["channel_slot"];
  rd.close();
  String sub_topic1 = "ntqueue/topic/terminal/" + defaultServiceType;
  String sub_topic2 = "ntqueue/topic/terminal/" + defaultServiceType + "/" + defaultChannel;
  mqttClient.subscribe(sub_topic1.c_str());
  mqttClient.subscribe(sub_topic2.c_str());
  Serial.print(sub_topic1); Serial.println(" Subscribed!");
  Serial.print(sub_topic2); Serial.println(" Subscribed!");
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived on topic: "); Serial.print(length); Serial.println(topic);
  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> rdDoc;
  DeserializationError rdError = deserializeJson(rdDoc, rd);
  if (rdError) {
    Serial.println(F("Failed to read file"));
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.println(F("Cannot extract json data."));
  }

  String statusTxt = doc["status"];
  Serial.printf("Queue Status: %s\n", statusTxt);
  if (statusTxt == "waiting") {
    int waitingCount = doc["rc"];
    String waitingStr = "Waiting: " + String(waitingCount);
    lcd.setCursor(0,0);
    lcd.print(waitingStr);
  }

  if (statusTxt == "calling") {
    strlcpy(tempToken, doc["token"], sizeof(tempToken));
    strlcpy(tempQueue, doc["queue_num"], sizeof(tempQueue));
    String callingStr = "Calling: " + String(tempQueue);
    lcd.setCursor(0,1);
    lcd.print(callingStr);
  }
  if (statusTxt == "ended") {
    Serial.println("Ended Process");
    /*String endingStr = "Ending: " + String(tempQueue);
    lcd.setCursor(0,1);
    lcd.print(endingStr);*/
  }
  if (statusTxt == "recall") {
    Serial.println("Recall Process");
    /*String recallStr = "Recall: " + String(tempQueue);
    lcd.setCursor(0,1);
    lcd.print(recallStr);*/
  }
}

// Buzzer
void buzzerBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
  delay(1000);
}

// LED
void ledToggle(int ledPin) {
  digitalWrite(ledPin, HIGH);
  delay(1000);
  digitalWrite(ledPin, LOW);
  delay(1000);
}

void setup() {
  delay(1000);
  Serial.begin(115200);

  // SPIFFs
  while (!Serial) continue;
  while (!SPIFFS.begin(true)) {
    Serial.println(F("Failed to initialize SPIFFS library"));
    delay(1000);
  }
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename, mycnf);

  // AutoConnect
  page_add.load(PAGE_ADD);
  page_results.load(PAGE_RESULTS);
  portal.join({ page_add, page_results });
  portal.on("/add", onAdd);
  portal.on("/results", onResults);
  String softAP = "NT-" + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  acConfig.apid = softAP;
  acConfig.psk = "123456789";
  portal.config(acConfig);
  if (portal.begin()) {
    wifiLocalIP = WiFi.localIP().toString();
    Serial.println("WiFi connected: " + wifiLocalIP);
    Serial.println("Mac Address: " + WiFi.macAddress());
    Serial.printf("mDNS responder : %s", softAP);
    if (MDNS.begin(softAP.c_str())) {
      MDNS.addService("http", "tcp", 80);
      Serial.println(" started");
    } else {
      Serial.println(" setting up failed");
    }

    mqttClient.setServer(mqtt_broker, mqtt_port);
    mqttClient.setCallback(mqttCallback);
  } else {
    Serial.println("Connection failed:" + String(WiFi.status()));
    Serial.println(F("Needs WiFi connection to start publishing messages"));
    while (true) {
      yield();
    }
  }

  // LED and Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(GREEN_LED, HIGH);

  // LCD
  lcd.init();
  lcd.backlight();
  scrollMessage(0, "Welcome to NT Queue System!", 150, totalColumns);
  lcd.print("Ready!!!");
}

void loop() {
  // AutoConnect
  portal.handleClient();

  // WiFi
  if (WiFi.status() == WL_IDLE_STATUS) {
    ESP.restart();
    delay(1000);
  }

  // MQTT
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> rdDoc;
  DeserializationError error = deserializeJson(rdDoc, rd);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration."));
  }

  // Keypad
  char key = keypad.getKey();
  if (key) {
    //lcd.clear();
    if (key == 'S') {
      Serial.println("Call pressed");
      requestData("S", "calling");
      ledToggle(RED_LED);
      buzzerBeep();
    }
    if (key == 'E') {
      Serial.println("End pressed");
      String endingStr = "Ending: " + String(tempQueue);
      lcd.setCursor(0,1);
      lcd.print(endingStr);
      publishData("E", "ended");
      ledToggle(RED_LED);
      buzzerBeep();
    }
    if (key == 'R') {
      Serial.println("Recall pressed");
      String recallStr = "Recall: " + String(tempQueue);
      lcd.setCursor(0,1);
      lcd.print(recallStr);
      publishData("R", "recall");
      ledToggle(RED_LED);
      buzzerBeep();
    }
    if (key == 'A') {
      String infoMsg = "AP: " + softAP + ", IP: " + wifiLocalIP;
      scrollMessage(0, infoMsg, 200, totalColumns);
    }
    if (key == 'B') {
      String x = rdDoc["employee_id"];
      String y = rdDoc["channel_slot"];
      String z = rdDoc["service_type"];
      String infoMsg = "ID: " + x + " SLOT: " + y + " SERVICE: " + z;
      scrollMessage(0, infoMsg, 200, totalColumns);
      rd.close();
    }
    if (key == 'C') {
      String sub_topic1 = "ntqueue/topic/terminal/1";
      String sub_topic2 = "ntqueue/topic/terminal/1/1";
      mqttClient.subscribe(sub_topic1.c_str());
      mqttClient.subscribe(sub_topic2.c_str());
      Serial.print(sub_topic1); Serial.println(" Subscribed!");
      Serial.print(sub_topic2); Serial.println(" Subscribed!");
    }
  }

  unsigned long currentTime = millis();
  if (currentTime - previousWaiting > timeIntervalWaiting) {
    previousWaiting = currentTime;
    // requestData("Z", "calling");
  }
}
