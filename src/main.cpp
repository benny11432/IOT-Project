#include <Arduino.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

rgb_lcd lcd;
AsyncWebServer server(80);


const char* ssid = "BensiPhone";
const char* password = "123455432";

const int ledG = 33, ledO = 25, ledR = 26, buzzer = 27, test = 4;
const int smokePin = 34, servoPin = 18;
float co2ppm = 0;
bool testFire = false;
Servo ventServo;

int displayIndex = 0;
const int numStatuses = 4;

// State 0 = NORMAL, 1 = WARNING, 2 = DANGER
int currentState = 0;

void setup() {
  Serial.begin(115200);

  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 255);

  pinMode(ledG, OUTPUT); pinMode(ledO, OUTPUT); pinMode(ledR, OUTPUT);
  pinMode(buzzer, OUTPUT); pinMode(test, INPUT_PULLUP); pinMode(smokePin, INPUT);
  ventServo.attach(servoPin);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("IP: " + WiFi.localIP().toString());

server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<h1>Greenhouse Dashboard</h1>";
    html += "<p>CO2: " + String(co2ppm) + " ppm</p>";
    if (currentState == 2) html += "<p style='color:red'>DANGER</p>";
    else if (currentState == 1) html += "<p style='color:orange'>WARNING</p>";
    else html += "<p style='color:green'>System OK</p>";
    request->send(200, "text/html", html);
});
server.begin();
  Serial.println("Web server started");

  digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
}

void loop() {
  static unsigned long normalTimer = 0;

  int sensorValue = analogRead(smokePin);
  co2ppm = map(sensorValue, 100, 3000, 400, 2000);
  testFire = (digitalRead(test) == LOW);

  if (testFire || co2ppm > 1200) currentState = 2;
  else if (co2ppm > 800) currentState = 1;
  else currentState = 0;

  if (currentState == 0) {
    if (millis() - normalTimer > 2000) {
      lcd.clear(); lcd.setCursor(0, 0);
      if (displayIndex == 0) lcd.print("Greenhouse OK");
      else if (displayIndex == 1) lcd.print(WiFi.status() == WL_CONNECTED ? "WiFi Connected" : "WiFi Failed");
      else if (displayIndex == 2) { lcd.print("Vent: CLOSED"); ventServo.write(0); }
      else { lcd.print("Air OK "); lcd.print(co2ppm, 0); lcd.print("ppm"); lcd.setCursor(0, 1); lcd.print("System OK"); }
      displayIndex = (displayIndex + 1) % numStatuses;
      normalTimer = millis();
    }
    digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
  }

  else if (currentState == 1) {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("WARNING CO2 High");
    lcd.setCursor(0, 1); lcd.print(co2ppm, 0); lcd.print("ppm");
    digitalWrite(ledO, HIGH); digitalWrite(ledG, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
    ventServo.write(90);
  }

  else if (currentState == 2) {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("DANGER Vent Open");
    lcd.setCursor(0, 1); lcd.print(co2ppm, 0); lcd.print("ppm");
    digitalWrite(ledR, HIGH); digitalWrite(ledG, LOW); digitalWrite(ledO, LOW); digitalWrite(buzzer, HIGH);
    ventServo.write(180);
  }
}
