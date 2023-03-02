#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <AutoConnect.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define RESET_BTN 32

struct myConfig {
  char employee_id[64];
  int channel_slot;
};

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
        "apply": "text",
        "global": true
      },
      {
        "name": "valueChannel",
        "type": "ACInput",
        "label": "Channel Slot",
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
    "title": "Channel Settings",
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
      }
    ]
  }
)";

const char PAGE_CONFIGS[] PROGMEM = R"(
  {
    "uri": "/configs",
    "title": "Channel Configuration",
    "menu": true,
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
      }
    ]
  }
)";

Button bestButton(13);
Button goodButton(35);
Button normalButton(34);
Button badButton(39);
Button worstButton(36);

// MQTT Broker
const char *mqtt_broker   = "172.1.1.79";
const char *mqtt_username = "admin";
const char *mqtt_password = "nt-rec00";
const char *pub_topic     = "ntqueue/topic/core";
const char *sub_topic     = "ntqueue/topic/score";
const int   mqtt_port     = 1883;

// Parameters 
bool isEnabled = false;
String tokens;
unsigned int serviceType;

// Configuration file
const char *filename = "/config.txt";
myConfig mycnf;

// Initisl PubSubClient
WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// Initial WiFiManager
AutoConnect       portal;
AutoConnectConfig config("Channel Slot - 1", "12345678");
AutoConnectAux    page_add;
AutoConnectAux    page_results;
AutoConnectAux    page_configs;

void deleteAllCredentials(void) {
  AutoConnectCredential credential;
  station_config_t config;
  uint8_t ent = credential.entries();

  Serial.printf("Credentials entries = %d\n", ent);

  while (ent--) {
    credential.load((int8_t)0, &config);
    credential.del((const char*)&config.ssid[0]);
  }
}

void onDisconnect() {
  Serial.println("Action button was pressed");

  Serial.println("Deleting credentials...: ");
  deleteAllCredentials();

  WiFi.disconnect(false, true);
  
  ESP.restart();
}

void loadConfiguration(const char *filename, myConfig &mycnf) {
  File ld = SPIFFS.open(filename);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, ld);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration."));
  }
  mycnf.channel_slot = doc["channel_slot"] | 1;
  strlcpy(mycnf.employee_id, doc["employee_id"] | "112233445566", sizeof(mycnf.employee_id));
  ld.close();
}

void saveConfiguration(const char *filename, const myConfig &mycnf) {
  SPIFFS.remove(filename);

  File wr = SPIFFS.open(filename, FILE_WRITE);
  if (!wr) {
    Serial.println(F("Failed to create file."));
  }

  StaticJsonDocument<256> doc;
  doc["employee_id"] = mycnf.employee_id;
  doc["channel_slot"] = mycnf.channel_slot;

  if (serializeJson(doc, wr) == 0) {
    Serial.println(F("Failed to write to file."));
  }
  wr.close();
}

void printFile(const char *filename) {
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();
  file.close();
}

void publishData(String empId, unsigned int channel_Slot, unsigned int queueScore) {
  Serial.printf("publishing data: %s, %d, %d\r\n", empId, channel_Slot, queueScore);

  const int capacity = JSON_OBJECT_SIZE(8);
  StaticJsonDocument<capacity> doc;
  doc["tokens"].set(tokens);
  doc["action"].set('C');
  doc["service_type"].set(serviceType);
  doc["channel_slot"].set(empId);
  doc["employee_id"].set(channel_Slot);
  doc["queue_score"].set(queueScore);

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  if (mqttClient.publish(pub_topic, buffer, n) == true) {
    Serial.println("Success sending message.");
  } else {
    Serial.println("Fail sending message.");
  }
  Serial.println();
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (mqttClient.connect(client_id.c_str(), mqtt_username,mqtt_password)) {
      Serial.println("MQTT Broker connected.");
    } else {
      Serial.print("failed with state: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
  mqttClient.subscribe(sub_topic);
  Serial.println("Subscribed!");
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  /*Serial.print(". Message: ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }*/
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
  tokens = (const char*)doc["token"];
  serviceType = doc["service_type"];

  Serial.print("Message : ");
  Serial.print(channelSlot1);
  Serial.print(channelSlot2);
  Serial.print(statusTxt);
  Serial.println();
  if (channelSlot1 == channelSlot2 && statusTxt == "ended") {
    isEnabled = true;
    Serial.print("change enabled: ");
    Serial.println(isEnabled);
  } else {
    Serial.println("Error: unenabled!");
  }
  Serial.println();
}

String onAdd(AutoConnectAux& aux, PageArgument& args) {
  aux["valueEmployee"].as<AutoConnectInput>().value = "112233445566";
  aux["valueChannel"].as<AutoConnectInput>().value = "0";
  return String();
}

String onResults(AutoConnectAux& aux, PageArgument& args) {
  String valueEmployee = args.arg("valueEmployee");
  int valueChannel = args.arg("valueChannel").toInt();

  aux["results"].as<AutoConnectText>().value = "Register Complete!!!";
  aux["resEmployee"].as<AutoConnectText>().value = "Employee ID: " + valueEmployee;
  aux["resChannel"].as<AutoConnectText>().value = "Channel Slot: " + String(valueChannel);

  strlcpy(mycnf.employee_id, valueEmployee.c_str(), sizeof(mycnf.employee_id));
  mycnf.channel_slot = valueChannel;
  Serial.println(F("Saving configuration..."));
  saveConfiguration(filename, mycnf);
  return String();
}

String onConfigs(AutoConnectAux& aux, PageArgument& args) {
  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rd);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration."));
  }

  String empID = doc["employee_id"];
  String channel_Slot = doc["channel_slot"];

  aux["results"].as<AutoConnectText>().value = "Your Configurations!!!";
  aux["resEmployee"].as<AutoConnectText>().value = "Employee ID: " + empID;
  aux["resChannel"].as<AutoConnectText>().value = "Channel Slot: " + channel_Slot;

  rd.close();

  return String();
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  while (!Serial) continue;

  while (!SPIFFS.begin()) {
    Serial.println(F("Failed to initialize SPIFFS library"));
    delay(1000);
  }

  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename, mycnf);

  pinMode(RESET_BTN, INPUT);

  bestButton.begin();
  goodButton.begin();
  normalButton.begin();
  badButton.begin();
  worstButton.begin();

  page_add.load(PAGE_ADD);
  page_results.load(PAGE_RESULTS);
  page_configs.load(PAGE_CONFIGS);
  portal.join({ page_add, page_results, page_configs });
  portal.on("/add", onAdd);
  portal.on("/results", onResults);
  portal.on("/configs", onConfigs);
  portal.config(config);
  if (portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());

    Serial.print("mDNS responder : ");
    if (MDNS.begin("channel_slot1")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("started");
    } else {
      Serial.println("setting up failed");
    }

    mqttClient.setServer(mqtt_broker, mqtt_port);
    mqttClient.setCallback(mqttCallback);
  } else {
    Serial.println("connection failed:" + String(WiFi.status()));
    Serial.println("Needs WiFi connection to start publishing messages");
    while (true) {
      yield();
    }
  }
}

void loop() {
  portal.handleClient();
  if (WiFi.status() == WL_IDLE_STATUS) {
    ESP.restart();
    delay(1000);
  }

  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  File rd = SPIFFS.open(filename);
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rd);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration."));
  }
  rd.close();

  //if (digitalRead(RESET_BTN) == LOW) {
  //  onDisconnect();
  //}
  
  if (bestButton.isReleased()) {
    if (isEnabled) {
      Serial.println("Score: Best!");
      publishData(doc["employee_id"], doc["channel_slot"], 5);
      isEnabled = !isEnabled;
    }
  }
  else if (goodButton.isReleased()) {
    if (isEnabled) {
      Serial.println("Score: Good!");
      publishData(doc["employee_id"], doc["channel_slot"], 4);
      isEnabled = !isEnabled;
    }
  }
  else if (normalButton.isReleased()) {
    if (isEnabled) {
      Serial.println("Score: Normal!");
      publishData(doc["employee_id"], doc["channel_slot"], 3);
      isEnabled = !isEnabled;
    }
  }
  else if (badButton.isReleased()) {
    if (isEnabled) {
      Serial.println("Score: Bad!");
      publishData(doc["employee_id"], doc["channel_slot"], 2);
      isEnabled = !isEnabled;
    }
  }
  else if (worstButton.isReleased()) {
    if (isEnabled) {
      Serial.println("Score: Worst!");
      publishData(doc["employee_id"], doc["channel_slot"], 1);
      isEnabled = !isEnabled;
    }
  }
}
