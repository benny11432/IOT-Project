/*components:
-LCD
-Green LED
-Orange LED
-Red LED
-ESP32
-Solar Panel 
-Motor
-Smoke detector / Co2
-GSM Module
-Buzzer

sudocode:

system in normal condition should roll through status. showing -

*Green power LED
*Solar voltage
*Vent(motor status)
*Air quality
*GSM Status

other conditions -
*Bad air quality, flash orange status LED, open servo motor, beep the buzzer,
 Message via GSM, show on LCD
*Fire / smoke / heat detected, Flash all LED, play tone 3 on buzzer,
 open roof vent, close floor vent, GSM, show on LCD
*/

#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// Blynk credentials
char auth[] = "YourAuthToken";    // Replace with your Blynk Auth Token
char ssid[] = "BensiPhone";         // WiFi SSID
char pass[] = "123455432";     // WiFi Password

#define BLYNK_TEMPLATE_ID "TMPL48_wnBOKJ"
#define BLYNK_TEMPLATE_NAME "IOT APP"

// LCD object
rgb_lcd lcd;

// Rolling display timing
unsigned long lastChange = 0;
int displayIndex = 0;
const int numStatuses = 4;

// Pins for LEDs, buzzer, etc.
const int ledG = 33;    // Green
const int ledO = 25;    // Orange
const int ledR = 26;    // Red
const int buzzer = 27;
const int test = 4;     // Test/fire input

void setup() {
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);

  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 255);

  pinMode(ledG, OUTPUT);
  pinMode(ledO, OUTPUT);
  pinMode(ledR, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(test, INPUT_PULLUP);

  digitalWrite(ledG, HIGH);  // Green LED on by default
  digitalWrite(ledO, LOW);
  digitalWrite(ledR, LOW);
  digitalWrite(buzzer, LOW);
}

void loop() {
  Blynk.run();

  // Simple fire/test alarm (active low)
  if (digitalRead(test) == LOW) {
    // LCD, LEDs, buzzer for alert
    lcd.setCursor(0, 0);
    lcd.print("FIRE! *****     ");
    digitalWrite(ledO, HIGH);
    digitalWrite(ledR, HIGH);
    digitalWrite(buzzer, HIGH);

    Blynk.virtualWrite(V0, "ALERT: FIRE!");
    delay(500);

    lcd.setCursor(0, 0);
    lcd.print("                ");
    digitalWrite(ledO, LOW);
    digitalWrite(ledR, LOW);
    digitalWrite(buzzer, LOW);

    delay(500);
  } else {
    // Normal rolling system status on LCD
    if (millis() - lastChange > 2000) {
        lcd.clear();
        if (displayIndex == 0) {
            lcd.print("Vent: Closed");
            Blynk.virtualWrite(V0, "Vent: Closed");
        } else if (displayIndex == 1) {
            lcd.print("Air: Good");
            Blynk.virtualWrite(V0, "Air: Good");
        } else if (displayIndex == 2) {
            lcd.print("GSM: Online");
            Blynk.virtualWrite(V0, "GSM: Online");
        } else if (displayIndex == 3) {
            lcd.print("System OK");
            Blynk.virtualWrite(V0, "System OK");
        }

        displayIndex = (displayIndex + 1) % numStatuses;
        lastChange = millis();
    }
    // LEDs for normal state
    digitalWrite(ledG, HIGH);
    digitalWrite(ledO, LOW);
    digitalWrite(ledR, LOW);
    digitalWrite(buzzer, LOW);
  }
}
