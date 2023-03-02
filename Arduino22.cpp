String formattedDate = "2018-04-30T16:00:13Z";
int splitT, firstDash, secondDash;
int mday, month, year;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello, ESP32!");

  firstDash = formattedDate.indexOf("-");
  year = formattedDate.substring(0, firstDash).toInt();
  Serial.println(year);

  secondDash = formattedDate.indexOf("-", firstDash+1);
  mday = formattedDate.substring(secondDash+1).toInt();
  Serial.println(mday);

  month = formattedDate.substring(firstDash+1, secondDash).toInt();
  Serial.println(month);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10); // this speeds up the simulation
}
