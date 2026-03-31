#include <Arduino.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

#define GSM_RX 16
#define GSM_TX 17
#define PHONE "+353830423660"

bool smsSent = false;

rgb_lcd lcd;
AsyncWebServer server(80);

const char* ssid     = "BensiPhone";
const char* password = "123455432";

const int ledG = 33, ledO = 25, ledR = 26, buzzer = 27, test = 4;
const int smokePin = 34, servoPin = 18;
float co2ppm  = 0;
bool testFire = false;
Servo ventServo;

int displayIndex      = 0;
const int numStatuses = 5;
int currentState      = 0;

String cachedSimStatus   = "Checking...";
String cachedNetworkName = "Checking...";
unsigned long lastGsmCheck = 0;
const unsigned long GSM_REFRESH_MS = 30000;

// ─── Non-blocking SMS state machine ──────────────────────────────────────────
enum SmsState { SMS_IDLE, SMS_WAIT_CMGF, SMS_WAIT_CMGS, SMS_WAIT_DONE };
SmsState smsState      = SMS_IDLE;
unsigned long smsTimer = 0;
String pendingSmsMsg   = "";

// ─── GSM INIT ────────────────────────────────────────────────────────────────
void initGSM() {
  delay(3000);
  while (Serial2.available()) Serial2.read();

  Serial2.print("AT\r");
  delay(1000);
  while (Serial2.available()) Serial.write(Serial2.read());

  Serial2.print("AT+CMGF=1\r");
  delay(1000);
  while (Serial2.available()) Serial.write(Serial2.read());

  Serial.println("Waiting for network...");
  for (int i = 0; i < 15; i++) {
    Serial2.print("AT+CREG?\r");
    delay(1000);
    String resp = "";
    while (Serial2.available()) resp += (char)Serial2.read();
    Serial.println(resp);
    if (resp.indexOf("+CREG: 0,1") >= 0 || resp.indexOf("+CREG: 0,5") >= 0) {
      Serial.println("Network registered!");
      break;
    }
  }

  Serial.println("GSM Init Done");
}

// ─── NON-BLOCKING SMS ────────────────────────────────────────────────────────
void queueSMS(String message) {
  if (smsState != SMS_IDLE) return;
  pendingSmsMsg = message;
  smsState      = SMS_WAIT_CMGF;
  smsTimer      = millis();
  Serial2.print("AT+CMGF=1\r");
  Serial.println("SMS queued: " + message);
}

void tickSMS() {
  switch (smsState) {

    case SMS_IDLE:
      break;

    case SMS_WAIT_CMGF:
      if (millis() - smsTimer > 1000) {
        String r = "";
        while (Serial2.available()) r += (char)Serial2.read();
        if (r.indexOf("OK") >= 0) {
          Serial2.print("AT+CMGS=\"" + String(PHONE) + "\"\r");
          smsState = SMS_WAIT_CMGS;
        } else {
          // retry
          Serial2.print("AT+CMGF=1\r");
        }
        smsTimer = millis();
      }
      break;

    case SMS_WAIT_CMGS:
      if (millis() - smsTimer > 1000) {
        Serial2.print(pendingSmsMsg);
        Serial2.print("\r");
        Serial2.write(26);    // Ctrl+Z
        Serial2.println();    // flush
        smsState = SMS_WAIT_DONE;
        smsTimer = millis();
      }
      break;

    case SMS_WAIT_DONE:
      if (millis() - smsTimer > 4000) {
        String r = "";
        while (Serial2.available()) r += (char)Serial2.read();
        Serial.println("SMS Response: " + r);
        if (r.indexOf("+CMGS") >= 0)
          Serial.println("✓ SMS Delivered: " + pendingSmsMsg);
        else
          Serial.println("✗ SMS failed - check SIM credit/signal");
        smsState = SMS_IDLE;
      }
      break;
  }
}

// ─── GSM STATUS ──────────────────────────────────────────────────────────────
void refreshGsmStatus() {
  while (Serial2.available()) Serial2.read();

  Serial2.print("AT+CREG?\r");
  delay(1000);
  String regResponse = "";
  while (Serial2.available()) regResponse += (char)Serial2.read();
  Serial.println("CREG: " + regResponse);
  if (regResponse.indexOf("+CREG: 0,1") >= 0 || regResponse.indexOf("+CREG: 0,5") >= 0)
    cachedSimStatus = "Registered";
  else
    cachedSimStatus = "No Network";

  while (Serial2.available()) Serial2.read();

  Serial2.print("AT+COPS?\r");
  delay(1000);
  String copsResponse = "";
  while (Serial2.available()) copsResponse += (char)Serial2.read();
  Serial.println("COPS: " + copsResponse);
  int start = copsResponse.indexOf("\"");
  int end   = copsResponse.lastIndexOf("\"");
  if (start >= 0 && end > start)
    cachedNetworkName = copsResponse.substring(start + 1, end);
  else
    cachedNetworkName = "Unknown";
}

// ─── DASHBOARD HTML ──────────────────────────────────────────────────────────
String buildDashboard() {
  String wifiStatus = (WiFi.status() == WL_CONNECTED) ? "Connected" : "Disconnected";
  String stateLabel, stateColor;

  if (currentState == 2)      { stateLabel = "DANGER";  stateColor = "#e74c3c"; }
  else if (currentState == 1) { stateLabel = "WARNING"; stateColor = "#f39c12"; }
  else                        { stateLabel = "NORMAL";  stateColor = "#2ecc71"; }

  String ventPos = (currentState == 2) ? "Fully Open (180&deg;)" :
                   (currentState == 1) ? "Partially Open (90&deg;)" : "Closed (0&deg;)";

  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="refresh" content="5">
  <title>Greenhouse Monitor</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', sans-serif;
      background: linear-gradient(135deg, #1a4a1a 0%, #2d7a2d 50%, #1a4a1a 100%);
      min-height: 100vh; padding: 20px; color: #fff;
    }
    h1 { text-align: center; font-size: 2em; margin-bottom: 6px; text-shadow: 0 2px 4px rgba(0,0,0,0.4); letter-spacing: 2px; }
    .subtitle { text-align: center; font-size: 0.85em; opacity: 0.7; margin-bottom: 24px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; max-width: 900px; margin: 0 auto; }
    .card { background: rgba(0,0,0,0.35); border-radius: 14px; padding: 20px; border: 1px solid rgba(255,255,255,0.1); backdrop-filter: blur(6px); }
    .card h2 { font-size: 0.75em; text-transform: uppercase; letter-spacing: 1.5px; opacity: 0.65; margin-bottom: 10px; }
    .card .value { font-size: 2em; font-weight: 700; }
    .card .unit { font-size: 0.85em; opacity: 0.6; margin-top: 4px; }
    .status-badge { display: inline-block; padding: 6px 16px; border-radius: 20px; font-weight: 700; font-size: 1.1em; color: #fff; }
    .dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-right: 6px; }
    .dot-green  { background: #2ecc71; }
    .dot-red    { background: #e74c3c; }
    .dot-orange { background: #f39c12; }
    .footer { text-align: center; margin-top: 24px; font-size: 0.75em; opacity: 0.5; }
  </style>
</head>
<body>
  <h1>&#127807; Greenhouse Monitor</h1>
  <p class="subtitle">Auto-refreshes every 5 seconds</p>
  <div class="grid">

    <div class="card">
      <h2>System Status</h2>
      <span class="status-badge" style="background:)rawhtml";
  html += stateColor + "\">" + stateLabel + "</span>";

  html += R"rawhtml(
    </div>
    <div class="card">
      <h2>CO2 Level</h2>
      <div class="value">)rawhtml";
  html += String(co2ppm, 0);
  html += R"rawhtml( <span style="font-size:0.5em;opacity:0.7">ppm</span></div>
      <div class="unit">Normal &lt;800 &nbsp;&#183;&nbsp; Warning &lt;1200 &nbsp;&#183;&nbsp; Danger 1200+</div>
    </div>
    <div class="card">
      <h2>Vent Position</h2>
      <div class="value" style="font-size:1.3em;">)rawhtml";
  html += ventPos;
  html += R"rawhtml(</div>
    </div>
    <div class="card">
      <h2>WiFi</h2>
      <div class="value" style="font-size:1.2em;">
        <span class="dot )rawhtml";
  html += (WiFi.status() == WL_CONNECTED) ? "dot-green" : "dot-red";
  html += R"rawhtml("></span>)rawhtml";
  html += wifiStatus;
  html += R"rawhtml(</div>
      <div class="unit">Network: )rawhtml";
  html += ssid;
  html += R"rawhtml(</div>
      <div class="unit">IP: )rawhtml";
  html += (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "N/A";
  html += R"rawhtml(</div>
    </div>
    <div class="card">
      <h2>GSM / SMS</h2>
      <div class="value" style="font-size:1.2em;">
        <span class="dot )rawhtml";
  html += (cachedSimStatus == "Registered") ? "dot-green" : "dot-red";
  html += R"rawhtml("></span>)rawhtml";
  html += cachedSimStatus;
  html += R"rawhtml(</div>
      <div class="unit">Network: )rawhtml";
  html += cachedNetworkName;
  html += R"rawhtml(</div>
      <div class="unit">SMS Alerts: )rawhtml";
  html += smsSent ? "Sent" : "Standby";
  html += R"rawhtml(</div>
    </div>
    <div class="card">
      <h2>Test Mode</h2>
      <div class="value" style="font-size:1.2em;">
        <span class="dot )rawhtml";
  html += testFire ? "dot-orange" : "dot-green";
  html += R"rawhtml("></span>)rawhtml";
  html += testFire ? "ACTIVE" : "Inactive";
  html += R"rawhtml(</div>
      <div class="unit">Manual fire trigger button</div>
    </div>

  </div>
  <p class="footer">Greenhouse IoT System &mdash; ESP32 &mdash; ATU Galway</p>
</body>
</html>
)rawhtml";

  return html;
}

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  initGSM();

  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 255);

  pinMode(ledG, OUTPUT); pinMode(ledO, OUTPUT); pinMode(ledR, OUTPUT);
  pinMode(buzzer, OUTPUT); pinMode(test, INPUT_PULLUP); pinMode(smokePin, INPUT);
  ventServo.attach(servoPin);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nIP: " + WiFi.localIP().toString());
  else
    Serial.println("\nWiFi Failed - continuing without it");

  refreshGsmStatus();
  lastGsmCheck = millis();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", buildDashboard());
  });
  server.begin();
  Serial.println("Web server started");

  digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW);
  digitalWrite(ledR, LOW);  digitalWrite(buzzer, LOW);
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
  static unsigned long normalTimer = 0;

  tickSMS(); // non-blocking, runs every iteration

  int sensorValue = analogRead(smokePin);
  co2ppm   = map(sensorValue, 100, 3000, 400, 2000);
  testFire = (digitalRead(test) == LOW);

  if (testFire || co2ppm > 1200)  currentState = 2;
  else if (co2ppm > 800)          currentState = 1;
  else                            currentState = 0;

  // ── NORMAL ──
  if (currentState == 0) {
    smsSent = false;
    if (millis() - normalTimer > 2000) {
      lcd.clear();
      if (displayIndex == 0) {
        lcd.setCursor(0, 0); lcd.print("System Within");
        lcd.setCursor(0, 1); lcd.print("Optimal Params");
      } else if (displayIndex == 1) {
        if (WiFi.status() == WL_CONNECTED) {
          lcd.setCursor(0, 0); lcd.print("WiFi Connected");
          lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
        } else {
          lcd.setCursor(0, 0); lcd.print("WiFi Failed");
        }
      } else if (displayIndex == 2) {
        lcd.setCursor(0, 0); lcd.print(cachedSimStatus);
        lcd.setCursor(0, 1); lcd.print(cachedNetworkName);
      } else if (displayIndex == 3) {
        lcd.setCursor(0, 0); lcd.print("Vent: CLOSED");
        ventServo.write(0);
      } else {
        lcd.setCursor(0, 0); lcd.print("Air OK ");
        lcd.print(co2ppm, 0); lcd.print("ppm");
        lcd.setCursor(0, 1); lcd.print("System OK");
      }
      displayIndex = (displayIndex + 1) % numStatuses;
      normalTimer  = millis();
    }
    digitalWrite(ledG, HIGH); digitalWrite(ledO, LOW);
    digitalWrite(ledR, LOW);  digitalWrite(buzzer, LOW);
  }

  // ── WARNING ──
  else if (currentState == 1) {
    if (!smsSent && smsState == SMS_IDLE) {
      queueSMS("WARNING: CO2 " + String(co2ppm) + "ppm - Vent partially opened!");
      smsSent = true;
    }
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WARNING CO2 High");
    lcd.setCursor(0, 1); lcd.print(co2ppm, 0); lcd.print("ppm");
    digitalWrite(ledO, HIGH); digitalWrite(ledG, LOW);
    digitalWrite(ledR, LOW);  digitalWrite(buzzer, LOW);
    ventServo.write(90);
  }

  // ── DANGER ──
  else if (currentState == 2) {
    if (!smsSent && smsState == SMS_IDLE) {
      queueSMS("DANGER: CO2 " + String(co2ppm) + "ppm - Vent fully opened!");
      smsSent = true;
    }
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("DANGER Vent Open");
    lcd.setCursor(0, 1); lcd.print(co2ppm, 0); lcd.print("ppm");
    digitalWrite(ledR, HIGH); digitalWrite(ledG, LOW);
    digitalWrite(ledO, LOW);  digitalWrite(buzzer, HIGH);
    ventServo.write(180);
  }
}