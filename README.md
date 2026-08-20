# Water Sense questionnaire

An Arduino questionnaire flow for a Nano 33 BLE Sense Rev2, Grove Shield for Arduino Nano v1.3, Grove 1.2-inch IPS Display, one Grove Button (P), and one Grove Rotary Angle Sensor (P).

## Hardware wiring

Set the Grove Shield’s voltage selector to **3.3 V** before fitting it to the Nano 33 BLE Sense Rev2.

| Component | Shield port | Pins used |
| --- | --- | --- |
| Grove 1.2-inch IPS Display | Digital D2 | D2 = clock (yellow), D3 = data (white) |
| Grove Button (P) | Digital D4 | D4 |
| Grove Rotary Angle Sensor (P) | Analog A0 | A0 |

The screen uses both signal wires on one Grove digital port. The other two buttons are not used.

## Arduino IDE setup

1. Install the **Arduino Mbed OS Nano Boards** platform in Boards Manager and select **Arduino Nano 33 BLE**. This is the compiler target used by the Nano 33 BLE Sense Rev2.
2. Install **Adafruit GFX Library** with Library Manager; allow it to install its required **Adafruit BusIO** dependency.
3. Download and install the Seeed display driver from [Arduino_ST7789_Fast](https://github.com/limengdu/Arduino_ST7789_Fast) using **Sketch → Include Library → Add .ZIP Library…**.
4. Open `WaterSenseQuestionnaire/WaterSenseQuestionnaire.ino`, select the correct serial port, and compile/upload.

The sketch does not transmit or persist data. It retains the current questionnaire answers and generated readings in an in-memory `Observation` struct until the board resets or a future restart flow replaces them.

