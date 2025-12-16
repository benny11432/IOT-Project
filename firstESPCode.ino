
#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFi.h>
//#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>


/*#define BLYNK_TEMPLATE_ID "TMPL48_wnBOKJ"
#define BLYNK_TEMPLATE_NAME "IOT APP"
#define BLYNK_AUTH_TOKEN "X2qyoRar3Jh0gQMgzU74BKXwIlATx-bM"


char ssid[] = "BensiPhone";
char pass[] = "123455432";*/


rgb_lcd lcd;
unsigned long lastChange = 0;
int displayIndex = 0;
const int numStatuses = 4;


const int ledG = 33, ledO = 25, ledR = 26, buzzer = 27, test = 4;
const int smokePin = 34, servoPin = 18;
float co2ppm = 0;
Servo ventServo;


void setup() {
  Serial.begin(115200);
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);  // ← FIXED: Correct parameter
 
  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 255);
 
  pinMode(ledG, OUTPUT); pinMode(ledO, OUTPUT); pinMode(ledR, OUTPUT);
  pinMode(buzzer, OUTPUT); pinMode(test, INPUT_PULLUP); pinMode(smokePin, INPUT);
  ventServo.attach(servoPin);
 
  digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
}


void loop() { 
 
  int sensorValue = analogRead(smokePin);
  co2ppm = map(sensorValue, 100, 3000, 400, 2000);
  bool testFire = (digitalRead(test) == LOW);
 
  if (testFire || co2ppm > 1200) {
    digitalWrite(ledO, HIGH); digitalWrite(ledR, HIGH); digitalWrite(buzzer, HIGH);
    ventServo.write(180);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Dangerous Co2 Lvl");
    lcd.setCursor(0, 1); lcd.print("CO2:"); lcd.print(co2ppm); lcd.print("ppm");
   // Blynk.virtualWrite(V0, "BAD AIR! " + String(co2ppm) + "ppm");
    delay(2000);
    digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Vent Open:");
    lcd.setCursor(0, 1); lcd.print("Max");
    delay(2000);
  }
   else if (co2ppm > 800) {
    digitalWrite(ledO, HIGH); digitalWrite(ledR, LOW); digitalWrite(buzzer, HIGH);
    ventServo.write(90);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Air Qlty Poor");
    lcd.setCursor(0, 1); lcd.print("CO2:"); lcd.print(co2ppm); lcd.print("ppm");
   // Blynk.virtualWrite(V0, "BAD AIR! " + String(co2ppm) + "ppm");
    delay(3000);
    digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
    delay(1000);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Vent Open:");
    lcd.setCursor(0, 1); lcd.print("Half");
    delay(1000);
   }
   else {
    if (millis() - lastChange > 2000) {
      lcd.clear();
      if (displayIndex == 0) lcd.print("Greenhouse ok");
      else if (displayIndex == 1) {
  #ifdef ESP32
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi: Connected");
  } else {
    lcd.print("WiFi: Disconnected");
  }
  #endif
}
      else if (displayIndex == 2) { lcd.print("Vent: CLOSED"); ventServo.write(0); }
      else if (displayIndex == 3) {
        lcd.print("Air:"); lcd.print(co2ppm, 0); lcd.print("ppm");
        lcd.setCursor(0, 1); lcd.print("System OK");
       // Blynk.virtualWrite(V0, "Air OK " + String(co2ppm) + "ppm");
      }
      displayIndex = (displayIndex + 1) % numStatuses;
      lastChange = millis();
    }
    digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
  }
}