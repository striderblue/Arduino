#include "DFRobotDFPlayerMini.h"
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>

#if !defined(UBRR1H)
#include <SoftwareSerial.h>
SoftwareSerial mySerial(D7, D8); // RX, TX
#endif

#define BUTTON_PIN_pause  D3     // Play_Pause Button
#define BUTTON_PIN_playnext  D5     // Next Button
#define BUTTON_PIN_volumeUp  D4     // VolUp Button
#define BUTTON_PIN_volumeDown  D6     // VolDown Button

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
U8X8_SSD1306_128X32_UNIVISION_SW_I2C u8x8(/* clock=D1*/ 5, /* data=D2*/ 4);

boolean oldState_pause = HIGH;
boolean oldState_playnext = HIGH;
boolean oldState_volumeUp = HIGH;
boolean oldState_volumeDown = HIGH;

int mode     = 0;    // Currently-active action mode
int playstate = 0;
int test = 0;
int tracks = 0;
int currtrack = 0;
int currvol = 0;
int volume = 0;
int volumeunder = 0;
int volumeover = 0;
DFRobotDFPlayerMini myDFPlayer;

void setup()
{
  pinMode(BUTTON_PIN_pause, INPUT_PULLUP);
  pinMode(BUTTON_PIN_playnext, INPUT_PULLUP);
  pinMode(BUTTON_PIN_volumeUp, INPUT_PULLUP);
  pinMode(BUTTON_PIN_volumeDown, INPUT_PULLUP);
  Serial.begin(115200);

#if !defined(UBRR1H)
  mySerial.begin(9600);
  myDFPlayer.begin(mySerial, true);
#else
  Serial1.begin(9600);
  myDFPlayer.begin(Serial1, true);
#endif
  delay(1000);

  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  u8x8.begin();
  Serial.println(u8x8.getRows());
  u8x8.setBusClock(400000);

  Serial.println("Setting volume to 15");
  myDFPlayer.volume(15);
  delay(100);
  currvol = myDFPlayer.readVolume();
  Serial.println(currvol);
  Lautstaerke();
  Track();
  delay(100);
}

void loop()
{
  boolean newState_pause = digitalRead(BUTTON_PIN_pause);
  boolean newState_playnext = digitalRead(BUTTON_PIN_playnext);
  boolean newState_volumeUp = digitalRead(BUTTON_PIN_volumeUp);
  boolean newState_volumeDown = digitalRead(BUTTON_PIN_volumeDown);

  // Play_Pause Button
  if ((newState_pause == LOW) && (oldState_pause == HIGH)) {
    // Short delay to debounce button.
    delay(20);
    // Check if button is still low after debounce.
    newState_pause = digitalRead(BUTTON_PIN_pause);
    if (newState_pause == LOW) {     // Yes, still low
      mode = 1; // Set mode for Switch
    }
  }
  // Play Next Button
  if ((newState_playnext == LOW) && (oldState_playnext == HIGH)) {
    // Short delay to debounce button.
    delay(20);
    // Check if button is still low after debounce.
    newState_playnext = digitalRead(BUTTON_PIN_playnext);
    if (newState_playnext == LOW) {     // Yes, still low
      mode = 2; // Set mode for Switch
    }
  }
  // volumeDown Button
  if ((newState_volumeDown == LOW) && (oldState_volumeDown == HIGH)) {
    // Short delay to debounce button.
    delay(20);
    // Check if button is still low after debounce.
    newState_volumeDown = digitalRead(BUTTON_PIN_volumeDown);
    if (newState_volumeDown == LOW) {     // Yes, still low
      mode = 3; // Set mode for Switch
    }
  }
  // volumeUp Button
  if ((newState_volumeUp == LOW) && (oldState_volumeUp == HIGH)) {
    // Short delay to debounce button.
    delay(20);
    // Check if button is still low after debounce.
    newState_volumeUp = digitalRead(BUTTON_PIN_volumeUp);
    if (newState_volumeUp == LOW) {     // Yes, still low
      mode = 4; // Set mode for Switch
    }
  }

  switch (mode) {          // Start an action...
    case 1:
      if (playstate == 1) {
        myDFPlayer.start();
        mode = 0;
        playstate = 2;
        Serial.println("Resume");
        Lautstaerke();
        Weiter();
        Track();
        delay(200);
        break;
      }
      else if (playstate == 2) {
        myDFPlayer.pause();
        mode = 0;
        playstate = 1;
        Serial.println("Pause");
        Lautstaerke();
        Pause();
        Track();
        delay(200);
        break;
      }
      else if (playstate == 0) {
        myDFPlayer.play(1);
        mode = 0;
        playstate = 2;
        Serial.println("Play");
        Lautstaerke();
        Play();
        Track();
        delay(200);
        break;
      }
    case 2:
      myDFPlayer.next();
      delay(100);
      mode     = 0;
      Serial.println("Next Track");
      tracks = myDFPlayer.readFileCounts();
      currtrack = myDFPlayer.readCurrentFileNumber();
      Serial.print(currtrack);
      Serial.print("/");
      Serial.print(tracks);
      Lautstaerke();
      Play();
      Track();
      break;
    case 3:

      myDFPlayer.volumeDown();
      mode = 0;
      Serial.println("Leiser");
      delay(100);
      test = myDFPlayer.readVolume();
      Serial.println(test);
      Lautstaerke();
      Track();
      if (playstate != 0) {
        if (playstate != 1) {
          Weiter();
        }
      } 
      break;
    case 4:
      myDFPlayer.volumeUp();
      mode = 0;
      Serial.println("Lauter");
      delay(100);
      test = myDFPlayer.readVolume();
      Serial.println(test);
      Lautstaerke();
      Track();
      if (playstate != 0) {
        if (playstate != 1) {
          Weiter();
        }
      }
      break;
  }
}
void Lautstaerke()
{
  int volume = myDFPlayer.readVolume();
  u8x8.setFont(u8x8_font_5x8_f);
  u8x8.setCursor(2, 0);
  volumeunder = volume - 1,
  u8x8.print(volumeunder);
  u8x8.setCursor(0, 1);
  u8x8.setFont(u8x8_font_8x13_1x2_f);
  u8x8.print(volume);
  u8x8.setFont(u8x8_font_5x8_f);
  u8x8.setCursor(2, 3);
  volumeover = volume + 1,
  u8x8.print(volumeover);
  for (int i = 0; i <= 5; i++) {
    u8x8.setCursor(4, i);
    u8x8.print("|");
  }
}
void Weiter()
{
  u8x8.setFont(u8x8_font_7x14_1x2_f);
  u8x8.setCursor(6, 0);
  u8x8.print("");
  u8x8.print("Playing");
}
void Play()
{
  u8x8.setFont(u8x8_font_7x14_1x2_f);
  u8x8.setCursor(6, 0);
  u8x8.print("");
  u8x8.print("Playing");
}
void Pause()
{
  u8x8.setFont(u8x8_font_7x14_1x2_f);
  u8x8.setCursor(6, 0);
  u8x8.print("");
  u8x8.print("Pause  ");
}
void Track()
{
  u8x8.setFont(u8x8_font_7x14_1x2_f);
  u8x8.setCursor(6, 2);
  u8x8.print(myDFPlayer.readCurrentFileNumber());
  u8x8.print(" / ");
  u8x8.print(myDFPlayer.readFileCounts());
}
