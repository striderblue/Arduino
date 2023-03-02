#include "FS.h"
#include "SD.h"
#include "SPI.h"
#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// function declaration
void postFileContent(const char * path );

#define FILE_LINE_LENGTH        81  // a line has 80 chars 
char txtLine[FILE_LINE_LENGTH];
char postdata [FILE_LINE_LENGTH];
bool readCondition = true;  // Has to be defined somewhere to trigger SD read

#ifdef ESP8266
const uint8_t chipSelect = 4;  //CS pin of the sd card reader
#endif
WiFiClient client;

void setup() {
  Serial.begin(115200);
  #ifdef ESP32
  if (!SD.begin()) {
  #elif defined(ESP8266)
  if (!SD.begin(chipSelect)) {
  #endif
    Serial.println("Card reader mount failed");
    return;
  }
  #ifdef ESP32
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  #endif
}

void loop() {
  //...... Your program structure
  if (client.connect("192.168.1.100", 80)) {
    if (readCondition == true) {
      postFileContent("/data_log.txt"); // Read file data_log.txt in Root directory
      readCondition = false; // reset condition
    }
    else {
      client.println("POST /readfile.php HTTP/1.1");
      client.println("Host: 192.168.1.100");
      client.println("User-Agent: Arduino/1.0");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(strlen(postdata));
      client.println("");
      client.println(postdata);
    }
    delay(1);
    client.stop();
  }
  //...... Your program structure

}

void postFileContent(const char* path) {
  Serial.print(F(" -- Reading entries from file = "));
  Serial.print(path);
  if (!SD.exists(path)) {
    Serial.println("ERROR: The required file does not exist.");
    return;
  }
  #ifdef ESP32
  File file = SD.open(path);
  #elif defined(ESP8266)
  File file = SD.open(path, FILE_READ); // FILE_READ is default so not realy needed but if you like to use this technique for e.g. write you need FILE_WRITE
  #endif
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  char c;
  uint8_t i = 0;

  while (file.available()) {
    c = file.read();
    if (c == '\n') { //Checks forline break
      txtLine[i] = '\0';
      Serial.print(F(" * "));
      Serial.println(txtLine); //This is where you get one line of file at a time.
      client.println("POST /readfile.php HTTP/1.1");
      client.println("Host: 192.168.1.100");
      client.println("User-Agent: Arduino/1.0");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(strlen(txtLine));
      client.println("");
      client.println(txtLine);
    }
    else if (c >= 32) {
      txtLine[i] = c;
      i++;
    }
  }
  file.close();
  Serial.println(F("DONE Reading"));
}
