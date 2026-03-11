#include <Arduino.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#define GSM_RX 16
#define GSM_TX 17
#define GSM_RST 5
#define PHONE "+353830423660"  // my number

bool smsSent = false;  // Stops repeat SMS spam


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
const int numStatuses = 5;

// State 0 = NORMAL, 1 = WARNING, 2 = DANGER
int currentState = 0;

void setup() {
  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
pinMode(GSM_RST, OUTPUT);
digitalWrite(GSM_RST, HIGH);
delay(3000);  // Wait for module to register on network
Serial2.println("AT");        // Handshake
delay(1000);
Serial2.println("AT+CMGF=1"); // Set SMS text mode
delay(1000);
Serial.println("GSM Ready");


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

  void sendSMS(String message) {
  Serial2.println("AT+CMGF=1");
  delay(500);
  Serial2.println("AT+CMGS=\"" + String(PHONE) + "\"");
  delay(500);
  Serial2.print(message);
  Serial2.write(26);  // Ctrl+Z sends the SMS
  delay(3000);
  Serial.println("SMS Sent: " + message);
}

  String getSimStatus() {
  Serial2.println("AT+CREG?");
  delay(500);
  String response = "";
  while (Serial2.available()) {
    response += (char)Serial2.read();
  }
  if (response.indexOf("+CREG: 0,1") >= 0 || response.indexOf("+CREG: 0,5") >= 0)
    return "SIM: Registered";
  else
    return "SIM: No Network";
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
    smsSent = false;

    if (millis() - normalTimer > 2000) {
      lcd.clear();
      if (displayIndex == 0) {
        lcd.setCursor(0, 0); lcd.print("System Within");
        lcd.setCursor(0, 1); lcd.print("Optimal Params");
      }
      else if (displayIndex == 1) {
        if (WiFi.status() == WL_CONNECTED) {
          lcd.setCursor(0, 0); lcd.print("WiFi Connected");
          lcd.setCursor(0, 1); lcd.print(ssid);
        } else {
          lcd.setCursor(0, 0); lcd.print("WiFi Failed");
        }
      }
      else if (displayIndex == 2) {
        String simStatus = getSimStatus();
        lcd.setCursor(0, 0); lcd.print(simStatus);
        lcd.setCursor(0, 1); lcd.print("GSM Module OK");
      }
      else if (displayIndex == 3) {
        lcd.setCursor(0, 0); lcd.print("Vent: CLOSED");
        ventServo.write(0);
      }
      else {
        lcd.setCursor(0, 0); lcd.print("Air OK ");
        lcd.print(co2ppm, 0); lcd.print("ppm");
        lcd.setCursor(0, 1); lcd.print("System OK");
      }
      displayIndex = (displayIndex + 1) % numStatuses;
      normalTimer = millis();
    }
    digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
  }

  else if (currentState == 1) {
    if (!smsSent) {
      sendSMS("WARNING: CO2 " + String(co2ppm) + "ppm - Vent partially opened!");
      smsSent = true;
    }
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("WARNING CO2 High");
    lcd.setCursor(0, 1); lcd.print(co2ppm, 0); lcd.print("ppm");
    digitalWrite(ledO, HIGH); digitalWrite(ledG, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
    ventServo.write(90);
  }

  else if (currentState == 2) {
    if (!smsSent) {
      sendSMS("DANGER: CO2 " + String(co2ppm) + "ppm - Vent fully opened!");
      smsSent = true;
    }
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("DANGER Vent Open");
    lcd.setCursor(0, 1); lcd.print(co2ppm, 0); lcd.print("ppm");
    digitalWrite(ledR, HIGH); digitalWrite(ledG, LOW); digitalWrite(ledO, LOW); digitalWrite(buzzer, HIGH);
    ventServo.write(180);
  }
}


