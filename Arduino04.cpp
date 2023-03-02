//-------------------------------------------------------------------------------------------------------------------------//

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <FS.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>


//------------------------------------------------------------------------------------------------------------------------//

#include <TridentTD_LineNotify.h>
char line_token[45] = "";

//------------------------------------------------------------------------------------------------------------------------//

#define D4 2 // ใช้ไฟ LED สีฟ้า ของบอร์ด MCU ESP8266 ให้มีสัญญาณไฟกระพริบ ตาม Code ที่เขียน
#define D7 13 // ใช้เป็นปุ่มกด เพื่อเข้า AP Config ได้ตามความต้องการของผู้ใช้

//------------------------------------------------------------------------------------------------------------------------//
char device[10] = "";
//------------------------------------------------------------------------------------------------------------------------//

char Tmin[10] = "";
char Tmax[10] = "";
String St_Tmin;
String St_Tmax;
float Tmin_;
float Tmax_;


char Hmin[10] = "";
char Hmax[10] = "";
String St_Hmin;
String St_Hmax;
float Hmin_;
float Hmax_;


//------------------------------------------------------------------------------------------------------------------------//

bool shouldSaveConfig1 = false;
void saveConfigCallback1 () {
  shouldSaveConfig1 = true;
  Serial.println("Should save  All Data config");



}
//-----------------------------------------------------------------------------------------------------------------------//

//Temp count Record
char Tcount[10] = "";
String St_Tcount;
int Tcount_;
int Tcount_1;

//Humid count Record
char Hcount[10] = "";
String St_Hcount;
int Hcount_;
int  Hcount_1;




//------------------------------------------------------------------------------------------------------------------------//
//*********************************************       void setup        **************************************************//
//------------------------------------------------------------------------------------------------------------------------//

void setup() {

  //-------IO NODE MCU Esp8266-------//
  pinMode(D4, OUTPUT);      //กำหนดโหมดใช้งานให้กับขา D0 เป็นขา สัญญาณไฟ ในสภาวะต่างๆ
  pinMode(D7, INPUT_PULLUP);//กำหนดโหมดใช้งานให้กับขา D3 เป็นขา กดปุ่ม ค้าง เพื่อตั้งค่า AP config

  //-----------------------------------------------------------------------------------------------------------------------//
  //read configuration from FS json
  Serial.println("mounting  FS...");

  if (SPIFFS.begin()) {

    //---------------------------------------------------------------------------------//

    Serial.println("mounted Data file system");


    if (SPIFFS.exists("/config1.json")) {
      //file exists, reading and loading
      Serial.println("reading Data config file");
      File configFile1 = SPIFFS.open("/config1.json", "r");

      if (configFile1) {
        Serial.println("opened Data config file");
        size_t size = configFile1.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf1(new char[size]);

        configFile1.readBytes(buf1.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf1.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed  json");
          strcpy(line_token, json["line_token"]);
          strcpy(device, json["device"]);
          strcpy(Tmin, json["Tmin"]);
          strcpy(Tmax, json["Tmax"]);
          strcpy(Hmin, json["Hmin"]);
          strcpy(Hmax, json["Hmax"]);
          strcpy(Tcount, json["Tcount"]);
          strcpy(Hcount, json["Hcount"]);
        } else {
          Serial.println("failed to load Data file json config");
        }
      }
    }
    //---------------------------------------------------------------------------------//



  } else {
    Serial.println("failed to mount FS");

  }
  //end read


  delay(2000);



  //-----------------------------------------------------------------------------------------------------------------------//
  WiFiManagerParameter custom_device("device", "device", device, 10);
  WiFiManagerParameter custom_line_token("LINE", "line_token", line_token, 45);

  WiFiManagerParameter custom_Tmin("Tmin", "Tmin", Tmin, 10);
  WiFiManagerParameter custom_Tmax("Tmax", "Tmax", Tmax, 10);

  WiFiManagerParameter custom_Hmin("Hmin", "Hmin", Hmin, 10);
  WiFiManagerParameter custom_Hmax("Hmax", "Hmax", Hmax, 10);

  WiFiManagerParameter custom_Tcount("Tcount", "Tcount", Tcount, 10);
  WiFiManagerParameter custom_Hcount("Hcount", "Hcount", Hcount, 10);
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback1);

  wifiManager.addParameter(&custom_device);
  wifiManager.addParameter(&custom_line_token);

  wifiManager.addParameter(&custom_Tmin);
  wifiManager.addParameter(&custom_Tmax);

  wifiManager.addParameter(&custom_Hmin);
  wifiManager.addParameter(&custom_Hmax);

  wifiManager.addParameter(&custom_Tcount);
  wifiManager.addParameter(&custom_Hcount);


  delay(2000);

  for (int i = 5; i > -1; i--) {  // นับเวลาถอยหลัง 5 วินาทีก่อนกดปุ่ม AP Config
    digitalWrite(D4, LOW);
    delay(500);
    digitalWrite(D4, HIGH);
    delay(500);
    Serial.print (String(i) + " ");
  }

  if (digitalRead(D7) == LOW) {
    digitalWrite(D4, LOW);
    Serial.println("Button Pressed");

    wifiManager.resetSettings();//ให้ล้างค่า SSID และ Password ที่เคยบันทึกไว้


    // wifiManager.autoConnect(); ใช้สร้างชื่อแอคเซสพอยต์อันโนมัติจาก ESP + ChipID

    if (!wifiManager.autoConnect("PUYIOT AP CONFIG")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }

  Serial.println(": Connected.......OK!)");
  strcpy(device, custom_device.getValue());
  strcpy(line_token, custom_line_token.getValue());

  strcpy(Tmin, custom_Tmin.getValue());
  strcpy(Tmax, custom_Tmax.getValue());

  strcpy(Hmin, custom_Hmin.getValue());
  strcpy(Hmax, custom_Hmax.getValue());

  strcpy(Tcount, custom_Tcount.getValue());
  strcpy(Hcount, custom_Hcount.getValue());

  delay(2000);

  if (shouldSaveConfig1) {
    //------------------------------------------------------------------

    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    //------------------------------------------------------------------
    Serial.println("saving device config");
    json["device"] = device;
    File configFile = SPIFFS.open("/config1.json", "w");
    //------------------------------------------------------------------
    Serial.println("saving line config");
    json["line_token"] = line_token;
    File configFile1 = SPIFFS.open("/config1.json", "w");
    //------------------------------------------------------------------

    Serial.println("saving Tmin config");
    json["Tmin"] = Tmin;
    File configFile2 = SPIFFS.open("/config1.json", "w");
    //------------------------------------------------------------------
    Serial.println("saving Tmax config");
    json["Tmax"] = Tmax;
    File configFile3 = SPIFFS.open("/config1.json", "w");
    //------------------------------------------------------------------


    Serial.println("saving Hmin config");
    json["Hmin"] = Hmin;
    File configFile4 = SPIFFS.open("/config1.json", "w");
    //------------------------------------------------------------------
    Serial.println("saving Hmax config");
    json["Hmax"] = Hmax;
    File configFile5 = SPIFFS.open("/config1.json", "w");


    Serial.println("saving Tcount config");
    json["Tcount"] = Tcount;
    File configFile6 = SPIFFS.open("/config1.json", "w");
    //------------------------------------------------------------------
    Serial.println("saving Hcount_ config");
    json["Hcount"] = Hcount;
    File configFile7 = SPIFFS.open("/config1.json", "w");








    if (!configFile1) {
      Serial.println("failed to open  config file for writing");

    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();

    json.printTo(configFile1);
    configFile1.close();

    json.printTo(configFile2);
    configFile2.close();

    json.printTo(configFile3);
    configFile3.close();

    json.printTo(configFile4);
    configFile4.close();

    json.printTo(configFile5);
    configFile5.close();

    json.printTo(configFile6);
    configFile6.close();


    json.printTo(configFile7);
    configFile7.close();
  }

  delay(2000);

  //-----------------------------------------------------------------------------------------------------------------------//

  Serial.println();
  Serial.print("local ip : ");
  delay(100);
  Serial.println(WiFi.localIP());
  delay(100);

  //-----------------------------------------------------------------------------------------------------------------------//
  //Print เพื่อเชคว่าค่าที่บันทึกไว้ยังอยู่หรือไม่
  Serial.println("เชคว่าค่าที่บันทึกไว้ยังอยู่หรือไม่");
  Serial.print("device =");
  Serial.println(device);

  Serial.print("line_token =");
  Serial.println(line_token);

  Serial.print("Tmin = ");
  Serial.println(Tmin);

  Serial.print("Tmax = ");
  Serial.println(Tmax);

  Serial.print("Hmin = ");
  Serial.println(Hmin);

  Serial.print("Hmax = ");
  Serial.println(Hmax);

  Serial.print("Tcount = ");
  Serial.println(Tcount);

  Serial.print("Hcount = ");
  Serial.println(Hcount);

  digitalWrite(D4, LOW);
  LINE.setToken(line_token);


  //----------------แปลงค่า Char >String > Float--------------------//
  // char Tmin[10] = "29.00";    char Tmax[10] = "35.00";  char Hmin[10] = "40.00";  char Hmax[10] = "65.00";
  // String St_Tmin;             String St_Tmax;           String St_Hmin;           String St_Hmax;
  // float Tmin_;                float Tmax_;              float Hmin_;              float Hmax_;

  Serial.println("เชคว่าค่าที่บันทึกได้ถูกแปลงเป็น Float แล้ว");

  for (int i = 0; i < sizeof(Tmin); i++)
    St_Tmin += Tmin[i];
  Tmin_ = St_Tmin.toFloat();
  Serial.print("Float Tmin = ");
  Serial.println(Tmin_);

  for (int i = 0; i < sizeof(Tmax); i++)
    St_Tmax += Tmax[i];
  Tmax_ = St_Tmax.toFloat();
  Serial.print("Float Tmax = ");
  Serial.println(Tmax_);


  for (int i = 0; i < sizeof(Hmin); i++)
    St_Hmin += Hmin[i];
  Hmin_ = St_Hmin.toFloat();
  Serial.print("Float Hmin = ");
  Serial.println(Hmin_);



  for (int i = 0; i < sizeof(Hmax); i++)
    St_Hmax += Hmax[i];
  Hmax_ = St_Hmax.toFloat();
  Serial.print("Float Hmax = ");
  Serial.println(Hmax_);

  Serial.println("เชคว่าค่าที่บันทึกได้ถูกแปลงเป็น Int แล้ว");
   //----------------แปลงค่า Char >String > Int--------------------//
  //char Tcount[6] = "10"; char Hcount[6] = "10";
  //String St_Tcount;      String St_Hcount;
  //int Tcount_;           int Hcount_;

  for (int i = 0; i < sizeof(Tcount); i++)
    St_Tcount += Tcount[i];
  Tcount_ = St_Tcount.toInt();
  Serial.print("Int Tcount = ");
  Serial.println(Tcount_);

  Tcount_1 = Tcount_ * 60;




  for (int i = 0; i < sizeof(Hcount); i++)
    St_Hcount += Hcount[i];
  Hcount_ = St_Hcount.toInt();
  Serial.print("Int Hcount = ");
  Serial.println(Hcount_);

  Hcount_1 = Hcount_ * 60;






  //-----------------------------------------------------------------------------------------------------------------------//

}

//------------------------------------------------------------------------------------------------------------------------//
//*********************************************   จบ  void setup        **************************************************//
//------------------------------------------------------------------------------------------------------------------------//




//------------------------------------------------------------------------------------------------------------------------//
//*********************************************       void Loop        ***************************************************//
//------------------------------------------------------------------------------------------------------------------------//
void loop(){
}

//------------------------------------------------------------------------------------------------------------------------//
//*********************************************      จบ void Loop       **************************************************//
//------------------------------------------------------------------------------------------------------------------------//
