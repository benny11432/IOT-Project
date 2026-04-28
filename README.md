Greenhouse Ventilation Monitoring System
This project is my second year IoT project based around an automated greenhouse ventilation monitoring system. The aim of the project was to create a low cost system which could monitor air quality and react when the conditions became unsafe. The system was designed to give clear feedback through LEDs, an LCD display, a web dashboard and GSM alerts.

Main Features
- Air quality / CO2 monitoring

- Automatic vent control using a servo motor

- LCD screen for live system updates

- WiFi dashboard for remote monitoring

- GSM SMS alerts for warning and danger states

- LED and buzzer indicators

- Manual test mode

Hardware Used
- ESP32

- Co2 sensor

- Servo motor

- RGB LCD display

- GSM module

- LEDs

- Buzzer

- Push button

- Breadboard and wires

Software Used
- Arduino IDE

- PlatformIO

- VS Code

- Arduino C++

- HTML and CSS

- GitHub

How it Works
The system reads the sensor value and converts it into a ppm style reading. From this, the project moves between three different states which are normal, warning and danger.

In normal, the vent stays closed and the system shows standard updates.

In warning, the vent opens partially and a warning alert can be sent.

In danger, the vent opens fully, the buzzer sounds, the red LED turns on and an SMS alert is sent.

This allows the system to react automatically without needing constant input from the user.

Web Dashboard
The project includes a web dashboard hosted by the ESP32. The dashboard was designed in a simple way so it is easy to navigate and understand. It uses a bubbly design with rounded sections and clear colours to show the current state of the system.

- The dashboard displays:

- System status

- CO2 level

- Vent position

- WiFi status

- GSM status

- Test mode
