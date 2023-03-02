#include <ModbusMaster.h>

#define MAX485_DE 22
#define MAX485_RE_NEG 23

ModbusMaster node1;

void preTransmission()
{
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission()
{
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

void setup() {
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  //Modbus communication runs at 9600 baud  
  Serial.begin(9600);
  node1.begin(1, Serial);
  node1.preTransmission(preTransmission);
  node1.postTransmission(postTransmission);
}

void loop() {
    uint8_t result; // 0-255
    result = node1.readInputRegisters(3927, 2);
    Serial.println(result);
    if (result == node1.ku8MBSuccess){
      Serial.print("VLL: ");
      Serial.println(node1.getResponseBuffer(0));
      /*Serial.print("V23: ");
      Serial.println(node.getResponseBuffer(11));
      Serial.print("V31: ");
      Serial.println(node.getResponseBuffer(25));
      Serial.print("VLN: ");
      Serial.println(node.getResponseBuffer(39));
      Serial.print("V12: ");
      Serial.println(node.getResponseBuffer(53));
      Serial.print("V1: ");
      Serial.println(node.getResponseBuffer(27));
      Serial.print("V2: ");
      Serial.println(node.getResponseBuffer(41));
      Serial.print("V3: ");
      Serial.println(node.getResponseBuffer(55));
      Serial.print("A: ");
      Serial.println(node.getResponseBuffer(13));
      Serial.print("PF: ");
      Serial.println(node.getResponseBuffer(07));*/
    } else 
      Serial.println("Fail");
  delay(100);
}
