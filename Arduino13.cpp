#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <AutoConnect.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define BUZZER_PIN 15

// Parameters
unsigned long previousWaiting = millis();
const unsigned long timeIntervalWaiting = 15000;

unsigned long lastAttempt = 0;
const unsigned long attempInterval = 3000;
bool reconnect = false;
int retry;

char tempToken[50] = "";
char tempQueue[5] = "";
char callerClientUrl[100] = "";
char subscribe_topic[100];
char clientId[100] = "";
bool isConfigured = false;
bool isEnabled = false;
bool isSubscribed = false;

// Buzzer
void buzzerBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
  delay(1000);
}

// MQTT Broker
const char *mqtt_broker   = "10.88.88.152";
const char *mqtt_username = "admin";
const char *mqtt_password = "nt@rec00";
const char *pub_topic     = "ntqueue/topic/core";
const int   mqtt_port     = 1883;

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// SPIFFs and AutoConnect
struct myConfig {
  char employee_id[64];
  char channel_slot[2];
  char service_type[2];
};

const char* filename = "/config.txt";
myConfig mycnf;

// Smile Buttons
class Button {
  private:
    bool _state;
    uint8_t _pin;
  
  public:
    Button(uint8_t pin) : _pin(pin) {}

    void begin() {
      pinMode(_pin, INPUT);
      _state = digitalRead(_pin);
    }

    bool isReleased() {
      bool v = digitalRead(_pin);
      if (v != _state) {
        _state = v;
        if (_state) {
          return true;
        }
      }
      return false;
    }
};

Button bestButton(13);
Button goodButton(35);
Button normalButton(34);
Button badButton(39);
Button worstButton(36);

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
        "type": "ACSelect",
        "label": "Channel Slot",
        "option": [
          "1","2","3","4","5",
          "6","7","8","9","10",
          "11","12","13","14","15"
        ],
        "selected": 1
      },
      {
        "name": "valueService",
        "type": "ACSelect",
        "label": "Service Number",
        "option": [
          "1","2","3","4","5",
          "6","7","8","9","10",
          "11","12","13","14","15"
        ],
        "selected": 1
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

  strlcpy(mycnf.channel_slot, doc["channel_slot"] | "1", sizeof(mycnf.channel_slot));
  strlcpy(mycnf.service_type, doc["service_type"] | "1", sizeof(mycnf.service_type));
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

void mqttUnsubscribed() {
  mqttClient.unsubscribe(subscribe_topic);
  isSubscribed = false;
}

void mqttSubscribed(const char *filename, const myConfig &mycnf) {
  const char *prefix_topic = "ntqueue/topic/smile/";
  const char *channelSlot = "";
  if (isSubscribed) {
    mqttUnsubscribed();
  }
  if (isConfigured) {
    channelSlot = mycnf.channel_slot;

    strlcpy(subscribe_topic, prefix_topic, sizeof(subscribe_topic));
    strlcat(subscribe_topic, channelSlot, sizeof(subscribe_topic));
    mqttClient.subscribe(subscribe_topic);
    Serial.print(subscribe_topic); Serial.println(" Subscribed!");
  } else {
    File rd = SPIFFS.open(filename);
    StaticJsonDocument<256> rdDoc;
    DeserializationError rdError = deserializeJson(rdDoc, rd);
    if (rdError) {
      Serial.println(F("Failed to read file"));
    }
    channelSlot = rdDoc["channel_slot"];
    rd.close();
    strlcpy(subscribe_topic, prefix_topic, sizeof(subscribe_topic));
    strlcat(subscribe_topic, channelSlot, sizeof(subscribe_topic));
    mqttClient.subscribe(subscribe_topic);
    Serial.print(subscribe_topic); Serial.println(" Subscribed!");
  }
}

String onAdd(AutoConnectAux& aux, PageArgument& args) {
  aux["valueEmployee"].as<AutoConnectInput>().value = "";
  aux["valueChannel"].as<AutoConnectInput>().value = "0";
  aux["valueService"].as<AutoConnectInput>().value = "0";
  return String();
}

String onResults(AutoConnectAux& aux, PageArgument& args) {
  String valueEmployee = args.arg("valueEmployee");
  String valueChannel = args.arg("valueChannel");
  String valueService = args.arg("valueService");

  aux["results"].as<AutoConnectText>().value = "Register Complete!!!";
  aux["resEmployee"].as<AutoConnectText>().value = "Employee ID: " + valueEmployee;
  aux["resChannel"].as<AutoConnectText>().value = "Channel Slot: " + valueChannel;
  aux["resService"].as<AutoConnectText>().value = "Service Type: " + valueService;

  strlcpy(mycnf.employee_id, valueEmployee.c_str(), sizeof(mycnf.employee_id));
  strlcpy(mycnf.channel_slot, valueChannel.c_str(), sizeof(mycnf.channel_slot));
  strlcpy(mycnf.service_type, valueService.c_str(), sizeof(mycnf.service_type));
  saveConfiguration(filename, mycnf);
  isConfigured = true;
  mqttSubscribed(filename, mycnf);
  return String();
}

// MQTT
void publishData(String empId, unsigned int channel_Slot, unsigned int queueScore) {
  // Serial.printf("publishing data: %s, %d, %d\r\n", empId, channel_Slot, queueScore);

  const int capacity = JSON_OBJECT_SIZE(8);
  StaticJsonDocument<capacity> doc;
  doc["token"] = tempToken;
  doc["action"] = "C";
  doc["channel_slot"] = channel_Slot;
  doc["employee_id"] = empId;
  doc["queue_score"] = queueScore;

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
    if (mqttClient.connect(client_id.c_str(), mqtt_username,mqtt_password)) {
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
  String defaultChannelSlot = rdDoc["channel_slot"];
  String sub_topic = "ntqueue/topic/smile/" + String(defaultChannelSlot);
  mqttClient.subscribe(sub_topic.c_str());
  Serial.print(sub_topic); Serial.println(F(" Subscribed!"));
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived on topic: "); Serial.println(topic);
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

  int channelSlot1 = rdDoc["channel_slot"];
  int channelSlot2 = doc["channel_slot"];
  String statusTxt = doc["status"];
  Serial.printf("Compare Incoming: %d and Store: %d, Status: %s\n", channelSlot1, channelSlot2, statusTxt);
  if (channelSlot1 == channelSlot2 && statusTxt == "ended") {
    isEnabled = true;
    strlcpy(tempToken, doc["token"], sizeof(tempToken));
    Serial.print("Change enabled: "); Serial.println(isEnabled);
  } else {
    Serial.println(F("Error: unenabled!"));
  }
  Serial.println();
}

void setup() {
  delay(1000);
  Serial.begin(115200);

  //Buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // SPIFFs
  while (!Serial) continue;
  while (!SPIFFS.begin()) {
    Serial.println(F("Failed to initialize SPIFFS library"));
    delay(1000);
  }
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename, mycnf);

  // Smile Buttons
  bestButton.begin();
  goodButton.begin();
  normalButton.begin();
  badButton.begin();
  worstButton.begin();

  // AutoConnect
  page_add.load(PAGE_ADD);
  page_results.load(PAGE_RESULTS);
  portal.join({ page_add, page_results });
  portal.on("/add", onAdd);
  portal.on("/results", onResults);

  const char *prefix_client = "nt-caller-";
  const char *suffix_client = WiFi.macAddress().c_str();
  strlcpy(clientId, prefix_client, sizeof(clientId));
  strlcat(clientId, suffix_client, sizeof(clientId));

  acConfig.apid = clientId;
  acConfig.psk = "123456789";
  portal.config(acConfig);

  if (portal.begin()) {
    sprintf(callerClientUrl, "%s/_ac", WiFi.localIP().toString().c_str());
    Serial.print("WiFi connected: "); Serial.println(callerClientUrl);
    Serial.println("Mac Address: " + WiFi.macAddress());
  } else {
    Serial.print("Connection failed: "); Serial.println(WiFi.status());
    Serial.println(F("Needs WiFi connection to start publishing messages"));
  }
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

  // Smile Buttons
  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rd);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration."));
  }
  rd.close();

  if (bestButton.isReleased()) {
    if (isEnabled) {
      Serial.println(F("Score: Best!"));
      publishData(doc["employee_id"], doc["channel_slot"], 5);
      buzzerBeep();
      isEnabled = !isEnabled;
    }
  }
  else if (goodButton.isReleased()) {
    if (isEnabled) {
      Serial.println(F("Score: Good!"));
      publishData(doc["employee_id"], doc["channel_slot"], 4);
      buzzerBeep();
      isEnabled = !isEnabled;
    }
  }
  else if (normalButton.isReleased()) {
    if (isEnabled) {
      Serial.println(F("Score: Normal!"));
      publishData(doc["employee_id"], doc["channel_slot"], 3);
      buzzerBeep();
      isEnabled = !isEnabled;
    }
  }
  else if (badButton.isReleased()) {
    if (isEnabled) {
      Serial.println(F("Score: Bad!"));
      publishData(doc["employee_id"], doc["channel_slot"], 2);
      buzzerBeep();
      isEnabled = !isEnabled;
    }
  }
  else if (worstButton.isReleased()) {
    if (isEnabled) {
      Serial.println(F("Score: Worst!"));
      publishData(doc["employee_id"], doc["channel_slot"], 1);
      buzzerBeep();
      isEnabled = !isEnabled;
    }
  }
}
