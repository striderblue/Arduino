#include <SPI.h>
#include <Ethernet.h>
#include <ThingerEthernet.h>
#include <ThingerESP8266.h>
#include <ESP8266WiFi.h>        // Include the Wi-Fi library

#define WIZRST 16 //Specify pin to use for reseting W5500
#define WIZCS 5 //Specify the pin for SPI CS

#define USERNAME "xxxxx"    //Thinger Account User Name
#define DEVICE_ID "xxxxx"     //Thinger Device ID ## NOTE This is the device serial number ##
ThingerEthernet thing(USERNAME, DEVICE_ID, DEVICE_ID);
//ThingerESP8266 thing(USERNAME, DEVICE_ID, DEVICE_ID);

const char* ssid     = "xxxxx";         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "xxxxx";     // The password of the Wi-Fi network
byte Enet = 1; //1 = Ethernet connection is available

byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 }; //MAC Address
//IPAddress ip = (192,168,1,200); //Fixed IP Address if required

void WizReset() {
  Serial.println("Reset Wiz W5500 Ethernet...");
  pinMode(WIZRST, OUTPUT);
  digitalWrite(WIZRST, HIGH);
  delay(500);
  digitalWrite(WIZRST, LOW);
  delay(50);
  digitalWrite(WIZRST, HIGH);
  delay(500);
}

void setup() {
  Ethernet.init(WIZCS);  // SPI CS Pin

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println();
  Serial.println("Hello..");
  SPI.begin();
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //Reset Wiznet W5500
  WizReset();

  // start & check the Ethernet connection:
  Serial.println("Starting Ethernet....");
  Ethernet.begin(mac);
  Enet = 1;
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    Enet = 0;
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
    Enet = 0;
  }
  //Check to see if Ethernet is connected, if not attempt connection with WiFi
  if (Enet == 0) {
    WiFi.disconnect(); //Remove previous SSID & Password
    WiFi.begin(ssid, password);             // Connect to the network
    Serial.print("WiFi Connecting to ");
    Serial.print(ssid); Serial.println(" ...");

    int i = 0;
    while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
      delay(1000);
      Serial.print(++i); Serial.print(' ');
    }

    Serial.println('\n');
    Serial.println("Connection established!");
    Serial.print("Local WiFi IP address: ");
    Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
  }
  // print your local Ethernet IP address if Ethernet is connected
  if (Enet == 1) {
    Serial.print("Local Ethernet IP address: ");
    Serial.println(Ethernet.localIP());
  }

  //Basic thinger.io resource (system timer)
  thing["millis"] >> [](pson & out) {
    out = millis();
  };
}

void loop() {
  thing.handle();
}
