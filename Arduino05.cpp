#include <Arduino.h>
#include <SPIFFS.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  unsigned int totalBytes = SPIFFS.totalBytes();
  unsigned int usedBytes = SPIFFS.usedBytes();

  Serial.println("File system info.");
  
  Serial.print("Total space: ");
  Serial.print(totalBytes);
  Serial.println("bytes.");

  Serial.print("Total Space used: ");
  Serial.print(usedBytes);
  Serial.println("bytes.");

  Serial.println();

  File file = SPIFFS.open("/test.txt");
  if (!file) {
    Serial.println("Failed to open file!");
    return;
  }

  Serial.println("Content of file:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
  
}

void loop() {
  // put your main code here, to run repeatedly:
}
