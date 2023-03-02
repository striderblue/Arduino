#include <Arduino.h>
#include <Wire.h>
#include <WiFiMulti.h>

#include <Artron_PCA9557.h>
#include <Artron_RTC.h>
#include <I2CEEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Audio.h>
#include "wm8960.h"

Artron_PCA9557 pca(0x19);
TaskHandle_t OutputTestTaskHandle = NULL;

void OutputTestTask(void*) {
  for(int pin=0;pin<4;pin++) {
    pca.pinMode(pin + 2, OUTPUT);
  }

  while(1) {
    for(int pin=0;pin<4;pin++) {
      pca.digitalWrite(pin + 2, HIGH);
      delay(500);
      pca.digitalWrite(pin + 2, LOW);
    }
    for(int pin=0;pin<4;pin++) {
      pca.digitalWrite(pin + 2, HIGH);
    }
    delay(500);
    for(int pin=0;pin<4;pin++) {
      pca.digitalWrite(pin + 2, LOW);
    }
    delay(500);
  }

  vTaskDelete(NULL);
}

TaskHandle_t InputTestTaskHandle = NULL;

#define D1 (4)
#define D2 (5)

#define A1 (1)
#define A2 (2)

void InputTestTask(void*) {
  pinMode(D1, INPUT_PULLUP);
  pinMode(D2, INPUT_PULLUP);

  while(1) {
    int d1_value = digitalRead(D1);
    int d2_value = digitalRead(D2);
    int a1_value = analogRead(A1);
    int a2_value = analogRead(A2);
    Serial.printf("[INPUT] D1=%d\tD2=%d\tA1=%d\tA2=%d\n", d1_value, d2_value, a1_value, a2_value);

    delay(1000);
  }
  
  vTaskDelete(NULL);
}

TaskHandle_t RTCTestTaskHandle = NULL;

Artron_RTC rtc(&Wire);

void RTCTestTask(void*) {
  while (!rtc.begin()) {
    Serial.println("Init RTC fail !!!");
    delay(500);
  }
    
  struct tm timeinfo_write = {
    .tm_sec = 0,
    .tm_min = 38,
    .tm_hour = 16,
    .tm_mday = 10,
    .tm_mon = 11,
    .tm_year = 2020,
  };

  while (!rtc.write(&timeinfo_write)) {
    Serial.println("RTC write fail !!!");
    delay(500);
  }
  Serial.println("RTC writed.");

  while(1) {
    struct tm timeinfo_read = { 0 };
    if (!rtc.read(&timeinfo_read)) {
      Serial.println("RTC read fail !!!");
      delay(500);
      continue;
    }
    Serial.printf("[RTC] %d/%d/%d %02d:%02d:%02d\n",
      timeinfo_read.tm_mday, timeinfo_read.tm_mon, timeinfo_read.tm_year + 1900,
      timeinfo_read.tm_hour, timeinfo_read.tm_min, timeinfo_read.tm_sec);
    delay(1000);
  }
  
  vTaskDelete(NULL);
}

TaskHandle_t EEPROMTestTaskHandle = NULL;
I2CEEPROM eep(0x50);

void EEPROMTestTask(void*) {
  Serial.println("[EEPROM] Write !");
  eep.print(0, "This data in EEPROM"); // Write String to I2C EEPROM start address 0
  
  delay(50); // wait EEPROM save data

  Serial.print("[EEPROM] Read: ");
  Serial.println(eep.readString(0)); // Read string from address 0
  
  // vTaskDelete(NULL);
}


/*
#define RS485_RX1  (18)
#define RS485_TX1  (17)
#define RS485_DIR1 (16)

#define RS485_RX2  (14)
#define RS485_TX2  (21)
#define RS485_DIR2 (47)
*/

#define RS485_RX2  (18)
#define RS485_TX2  (17)
#define RS485_DIR2 (16)

#define RS485_RX1  (14)
#define RS485_TX1  (21)
#define RS485_DIR1 (47)

#define MODE_SEND HIGH
#define MODE_RECV LOW

float temp1 = 0, humi1 = 0;
float temp2 = 0, humi2 = 0;

uint16_t CRC16(uint8_t *buf, int len) {  
  uint16_t crc = 0xFFFF;
  for (uint16_t pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];    // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      } else {                           // Else LSB is not set
        crc >>= 1;                    // Just shift right
      }
    }
  }

  return crc;
}

void readXY_MD02_1() {
  uint8_t buff[] = {
    0x02, // Devices Address
    0x04, // Function code
    0x00, // Start Address HIGH
    0x01, // Start Address LOW
    0x00, // Quantity HIGH
    0x02, // Quantity LOW
    0, // CRC LOW
    0  // CRC HIGH
  };

  uint16_t crc = CRC16(&buff[0], 6);
  buff[6] = crc & 0xFF;
  buff[7] = (crc >> 8) & 0xFF;

  digitalWrite(RS485_DIR1, MODE_SEND);
  //delay(1);
  Serial1.write(buff, sizeof(buff));
  Serial1.flush(); // wait MODE_SEND completed
  //delay(1);
  digitalWrite(RS485_DIR1, MODE_RECV);

  delay(100);
  
  if (Serial1.find("\x02\x04")) {
    uint8_t n = Serial1.read();
    if (n != 4) {
      Serial.printf("Error data size : %d\n", n);
      return;
    }

    temp1 = ((uint16_t)(Serial1.read() << 8) | Serial1.read()) / 10.0;
    humi1 = ((uint16_t)(Serial1.read() << 8) | Serial1.read()) / 10.0;
  } else {
    Serial.println("ERROR Timeout");
    return;
  }
}

void readXY_MD02_2() {
  uint8_t buff[] = {
    0x02, // Devices Address
    0x04, // Function code
    0x00, // Start Address HIGH
    0x01, // Start Address LOW
    0x00, // Quantity HIGH
    0x02, // Quantity LOW
    0, // CRC LOW
    0  // CRC HIGH
  };

  uint16_t crc = CRC16(&buff[0], 6);
  buff[6] = crc & 0xFF;
  buff[7] = (crc >> 8) & 0xFF;

  digitalWrite(RS485_DIR2, MODE_SEND);
  //delay(1);
  Serial2.write(buff, sizeof(buff));
  Serial2.flush(); // wait MODE_SEND completed
  //delay(1);
  digitalWrite(RS485_DIR2, MODE_RECV);

  delay(100);
  
  if (Serial2.find("\x02\x04")) {
    uint8_t n = Serial2.read();
    if (n != 4) {
      Serial.printf("Error data size : %d\n", n);
      return;
    }

    temp2 = ((uint16_t)(Serial2.read() << 8) | Serial2.read()) / 10.0;
    humi2 = ((uint16_t)(Serial2.read() << 8) | Serial2.read()) / 10.0;
  } else {
    Serial.println("ERROR Timeout");
    return;
  }
}

TaskHandle_t RS4851TestTaskHandle = NULL;

void RS4851TestTask(void*) {
  pinMode(RS485_DIR1, OUTPUT);
  digitalWrite(RS485_DIR1, MODE_RECV);
  Serial1.begin(9600, SERIAL_8N1, RS485_RX1, RS485_TX1);

  while(1) {
    readXY_MD02_1();
    Serial.printf("[RS485(1)] XY-MD02 (1): %.01f *C\t%.01f %%RH\n", temp1, humi1);
    delay(2000);
  }
  
  vTaskDelete(NULL);
}

TaskHandle_t RS4852TestTaskHandle = NULL;

void RS4852TestTask(void*) {
  pinMode(RS485_DIR2, OUTPUT);
  digitalWrite(RS485_DIR2, MODE_RECV);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX2, RS485_TX2);

  while(1) {
    readXY_MD02_2();
    Serial.printf("[RS485(2)] XY-MD02 (2): %.01f *C\t%.01f %%RH\n", temp2, humi2);
    delay(2000);
  }
  
  vTaskDelete(NULL);
}

TaskHandle_t EthernetTestTaskHandle = NULL;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "www.google.com";    // name address for Google (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(192, 168, 0, 1);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

// Variables to measure the speed
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
bool printWebData = true;  // set to false for better speed measurement

#define W5500_RST_PIN (15)

void EthernetTestTask(void*) {
  pinMode(W5500_RST_PIN, OUTPUT);
  digitalWrite(W5500_RST_PIN, HIGH);
  delay(100);

  Ethernet.init(10);  // W5500 CS pin

  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns);
  } else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.print("connecting to ");
  Serial.print(server);
  Serial.println("...");

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.print("connected to ");
    Serial.println(client.remoteIP());
    // Make a HTTP request:
    client.println("GET /search?q=arduino HTTP/1.1");
    client.println("Host: www.google.com");
    client.println("Connection: close");
    client.println();
  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
  beginMicros = micros();

  while(1) {
    // if there are incoming bytes available
    // from the server, read them and print them:
    int len = client.available();
    if (len > 0) {
      byte buffer[80];
      if (len > 80) len = 80;
      client.read(buffer, len);
      if (printWebData) {
        Serial.write(buffer, len); // show in the serial monitor (slows some boards)
      }
      byteCount = byteCount + len;
    }

    // if the server's disconnected, stop the client:
    if (!client.connected()) {
      endMicros = micros();
      Serial.println();
      Serial.println("disconnecting.");
      client.stop();
      Serial.print("Received ");
      Serial.print(byteCount);
      Serial.print(" bytes in ");
      float seconds = (float)(endMicros - beginMicros) / 1000000.0;
      Serial.print(seconds, 4);
      float rate = (float)byteCount / seconds / 1000.0;
      Serial.print(", rate = ");
      Serial.print(rate);
      Serial.print(" kbytes/second");
      Serial.println();

      // do nothing forevermore:
      while (true) {
        delay(1);
      }
    }
  }
  
  vTaskDelete(NULL);
}

TaskHandle_t AudioTestTaskHandle = NULL;
Audio audio;
WiFiMulti wifiMulti;

#define I2S_BCLK (41)
#define I2S_LRC  (40)
#define I2S_DOUT (3)
#define I2S_DIN  (42)

void AudioTestTask(void*) {
  WM89600_init();

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // 0...21

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP("Artron@Kit", "Kit_Artron");
  Serial.println("WiFi Connecting...");
  wifiMulti.run();
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    wifiMulti.run();
  }
  Serial.println("WiFi Connected");

  audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.mp3"); //  128k mp3

  while(1) {
    audio.loop();
    delay(1);
  }
  
  vTaskDelete(NULL);
}


void setup() {
  Serial.begin(115200);
  
  // I2C setup for Output work
  Wire.begin(8, 9, 100E3);
  SPI.begin(12, 13, 11, -1);

  EEPROMTestTask(NULL);

  //xTaskCreate(OutputTestTask, "OutputTestTask", 10 * 1024, NULL, 20, &OutputTestTaskHandle);
  //xTaskCreate(InputTestTask, "InputTestTask", 10 * 1024, NULL, 20, &InputTestTaskHandle);
  //xTaskCreate(RTCTestTask, "RTCTestTask", 10 * 1024, NULL, 20, &RTCTestTaskHandle);
  // xTaskCreate(EEPROMTestTask, "EEPROMTestTask", 10 * 1024, NULL, 20, &EEPROMTestTaskHandle);
  // xTaskCreate(RS4851TestTask, "RS4851TestTask", 10 * 1024, NULL, 20, &RS4851TestTaskHandle);
  // xTaskCreate(RS4852TestTask, "RS4852TestTask", 10 * 1024, NULL, 20, &RS4852TestTaskHandle);
  // xTaskCreate(EthernetTestTask, "EthernetTestTask", 10 * 1024, NULL, 20, &EthernetTestTaskHandle);
  xTaskCreate(AudioTestTask, "AudioTestTask", 10 * 1024, NULL, 20, &AudioTestTaskHandle);
}

void loop() {
  delay(1000);
}



// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}
