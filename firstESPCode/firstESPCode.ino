#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

rgb_lcd lcd;
WebServer server(80); //using http port named server

const char* ssid = "BensiPhone"; //assigning network info
const char* password = "123455432";

const int ledG = 33, ledO = 25, ledR = 26, buzzer = 27, test = 4;
const int smokePin = 34, servoPin = 18;
float co2ppm = 0;
bool testFire = false;
Servo ventServo;

unsigned long lastChange = 0; //positive 32 bit timer for display refresh since boot
int displayIndex = 0; //init on zero - state 0 ok, 1 wifi, 2 vent, 3 air, 4,5,6 coming later (GSM)

const int numStatuses = 4; //softcoded as i will be adding more states

void setup() {
  Serial.begin(115200); //make sure i set serial baud in lab to this
  
  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 255); //no R or G as this is blue display
  
  pinMode(ledG, OUTPUT); pinMode(ledO, OUTPUT); pinMode(ledR, OUTPUT);
  pinMode(buzzer, OUTPUT); pinMode(test, INPUT_PULLUP); pinMode(smokePin, INPUT);
  ventServo.attach(servoPin); //attachment for servo control modes, 0, 180, 90
  
  // WiFi Setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print("."); //added simply to show device is working and loading
  }
  Serial.println("IP: " + WiFi.localIP().toString());
  
  // Web Server
  server.on("/", [](){
    String html = "<h1>Autonomouse Greenhouse Monitoring Server</h1>";
    html += "<p>CO2: " + String(co2ppm) + " ppm</p>";
    if (co2ppm > 1200 || testFire) html += "<p style='color:red'>DANGEROUS CO2 LEVEL!</p>";
    else if (co2ppm > 800) html += "<p style='color:orange'>WARNING - CO2 LEVEL RISING!</p>";
    else html += "<p style='color:green'>System Healthy</p>";
    server.send(200, "text/html", html);
  });
  server.begin();
  Serial.println("Web server started");
  
  digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
}

void loop() {
  static unsigned long alertTimer = 0, normalTimer = 0;
  
  int sensorValue = analogRead(smokePin);
  co2ppm = map(sensorValue, 100, 3000, 400, 2000);
  testFire = (digitalRead(test) == LOW);

  if (testFire || co2ppm > 1200) {  // DANGER
    digitalWrite(ledO, HIGH); digitalWrite(ledR, HIGH); digitalWrite(buzzer, HIGH);
    ventServo.write(180);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Dangerous Co2 Lvl");
    lcd.setCursor(0, 1); lcd.print("CO2:"); lcd.print(co2ppm); lcd.print("ppm");
    if (millis() - alertTimer > 2000) {
      lcd.clear(); lcd.setCursor(0, 0); lcd.print("Vent Open:"); lcd.setCursor(0, 1); lcd.print("Max");
      if (millis() - alertTimer > 4000) { 
        digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW); 
        alertTimer = millis(); 
      }
    }
  }
  else if (co2ppm > 800) {  // WARNING
    digitalWrite(ledO, HIGH); digitalWrite(ledR, LOW); digitalWrite(buzzer, HIGH);
    ventServo.write(90);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Air Qlty Poor");
    lcd.setCursor(0, 1); lcd.print("CO2:"); lcd.print(co2ppm); lcd.print("ppm");
    if (millis() - alertTimer > 3000) { 
      digitalWrite(ledO, LOW); digitalWrite(buzzer, LOW); 
      ventServo.write(0); 
      alertTimer = millis(); 
    }
  }
  else {  // NORMAL
    if (millis() - normalTimer > 2000) {
      lcd.clear(); lcd.setCursor(0, 0);
      if (displayIndex == 0) lcd.print("Greenhouse ok");
      else if (displayIndex == 1) lcd.print(WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
      else if (displayIndex == 2) { lcd.print("Vent: CLOSED"); ventServo.write(0); }
      else { 
        lcd.print("Air OK "); lcd.print(co2ppm, 0); lcd.print("ppm"); 
        lcd.setCursor(0, 1); lcd.print("System OK"); 
      }
      displayIndex = (displayIndex + 1) % numStatuses;
      normalTimer = millis();
    }
    digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW); digitalWrite(ledR, LOW); digitalWrite(buzzer, LOW);
  }
  
  server.handleClient();  // Web requests
}
