// The keypad on this little microwave went out for some reason, and indtead of
// buying another one I descided to use some hardware laying around and make a
// basic controller as a replacment.

// I started to examine the circuitry in the microwave to understand how
// everything functioned.

// I started with an ESP32 running micropython, a rotary encoder and a 128x64
// OLED display for the UI, and an RTC so it didnt loose the time constantly.
// Later I wanted the esp32 for another experiment so I ported the code to
// Arduino.

// At this point everything was on a breadboard and humidity from the microwave
// not having the panel on kept causing issues with the breadboard. That and
// boiling over a deep fryer didnt help.

// Descided to make the project internal, drilled a hole in the front face of
// the microwave for the encoder and I thought it would be a good idea to use my
// impact drill to tighten the nut, well that broke the encoder. Went and
// upgraded to a 1x4 membrane keypad.

// I packed everything nice and neat in the cabinet and ran a usb cable out of
// the back.

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <DS3232RTC.h>
#include <Timezone.h>
#include <Wire.h>

#include "Adafruit_Keypad.h"
#include "RTClib.h"
#include "consola20pt.h"
#include "img_microwave_big.h"
#include "img_sadmac_big.h"
#include "img_void_glyph_big.h"

// Pin Defines
#define DOOR_PIN 8        // Door Switch
#define CIRCUIT_A_PIN 10  // Turntable, Light, Fan
#define CIRCUIT_B_PIN 9   // Magnetron is enabled when combined with CIRCUIT_A.
#define PIEZO_PIN 0       // Piezo Beeper

// Keypad
#define R1 3
#define C1 2
#define C2 6
#define C3 7
#define C4 1
#define KEYPAD_ROWS 1
#define KEYPAD_COLS 4
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3', '4'},
};
byte rowPins[KEYPAD_ROWS] = {3};
byte colPins[KEYPAD_COLS] = {2, 6, 7, 1};
Adafruit_Keypad kpd = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins,
                                      KEYPAD_ROWS, KEYPAD_COLS);

// Real Time Clock
DS3232RTC myRTC(false);
TimeChangeRule myDST = {"EDT", Second, Sun,
                        Mar,   2,      -240};  // Daylight time = UTC - 4 hours
TimeChangeRule mySTD = {"EST", First, Sun,
                        Nov,   2,     -300};  // Standard time = UTC - 5 hours
Timezone myTZ(myDST, mySTD);
TimeChangeRule *tcr;

// Display
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
bool failOLED = false;

// Check if i2c device is present.
bool probeI2C(byte addr) {
    Wire.begin();
    Wire.beginTransmission(addr);
    delay(1);
    return Wire.endTransmission() == 0;
}

// Fault Loop
// Displays Sad Macintosh Microwave
// Sounds alternating Tone to signify faiilure
bool fatalFail = false;
void fatalLoop() {
    SerialUSB.print(F("System Failure, missing RTC or OLED module!"));
    if (!failOLED) {
        display.clearDisplay();
        display.drawBitmap((OLED_WIDTH / 2) - (MICROWAVE_FRAME_WIDTH / 2),
                           (OLED_HEIGHT / 2) - (MICROWAVE_FRAME_HEIGHT / 2),
                           microwave, MICROWAVE_FRAME_WIDTH,
                           MICROWAVE_FRAME_HEIGHT, SSD1306_WHITE);
        display.drawBitmap((OLED_WIDTH / 2) - (MICROWAVE_FRAME_WIDTH / 2) +
                               (SADMAC_FRAME_WIDTH / 3),
                           (OLED_HEIGHT / 2) - (MICROWAVE_FRAME_HEIGHT / 2) +
                               (SADMAC_FRAME_HEIGHT / 3),
                           sadmac, SADMAC_FRAME_WIDTH, SADMAC_FRAME_HEIGHT,
                           SSD1306_WHITE);
        display.display();
    }
    for (;;) {
        tone(PIEZO_PIN, 300, 250);
        delay(250);
        tone(PIEZO_PIN, 600, 250);
        delay(250);
    }
}

// Perhaps too many variables, needs a clean up
bool bDuty;
bool bPause;
bool bCook;
bool bDoor;
bool bBlink;
bool bDone;
bool bStarted;
bool bStop;
bool bStart;
unsigned int iTone;
unsigned int iWait;
unsigned int iTones;
unsigned int iPower;
unsigned int iDone;
unsigned int iDuty;
unsigned int iCook;
unsigned int iBlink;
unsigned int iFormat;
unsigned long tBlink;
unsigned long tMagnetron;
const char *sFormat[] = {"%02d %02d", "%02d:%02d", "  :  "};
char sDisplay[6];
DateTime tCook;
DateTime tNone;
TimeSpan tDec(1);
TimeSpan tStep30(30);
TimeSpan tStep10(10);

void setup() {
    // Initial Pin States
    analogWriteResolution(10);
    pinMode(DOOR_PIN, INPUT_PULLUP);
    pinMode(CIRCUIT_A_PIN, OUTPUT);
    pinMode(CIRCUIT_B_PIN, OUTPUT);
    pinMode(PIEZO_PIN, OUTPUT);
    digitalWrite(CIRCUIT_A_PIN, HIGH);
    digitalWrite(CIRCUIT_B_PIN, HIGH);
    SerialUSB.begin(115200);
    SerialUSB.println(F("Initializing Infinity.mWaveController v2.0"));
    SerialUSB.print(F("Initializing RTC..."));
    if (!probeI2C(104)) {
        SerialUSB.println(F("Failed! Device not responding."));
        fatalFail = true;
    } else {
        myRTC.begin();
        setSyncProvider(myRTC.get);
        SerialUSB.println(F("Done"));
    }
    SerialUSB.print(F("Initializing OLED..."));
    if (!probeI2C(61)) {
        SerialUSB.println(F("Failed! Device not responding."));
        fatalFail = true;
    } else {
        if (!display.begin(SSD1306_SWITCHCAPVCC,
                           0x3D)) {  // Address 0x3D for 128x64
            SerialUSB.println(F("Failed! Cannot initialize display."));
            fatalFail = true;
            failOLED = true;
        } else {
            SerialUSB.println(F("Done"));
        }
    }
    if (fatalFail) {
        fatalLoop();
    }
    SerialUSB.println(F("Initialization Complete."));
    SerialUSB.println(F("Starting Infinity.mWaveController v2.0"));
    display.clearDisplay();
    // Draw Microwave
    display.drawBitmap((OLED_WIDTH / 2) - (MICROWAVE_FRAME_WIDTH / 2),
                       (OLED_HEIGHT / 2) - (MICROWAVE_FRAME_HEIGHT / 2),
                       microwave, MICROWAVE_FRAME_WIDTH, MICROWAVE_FRAME_HEIGHT,
                       SSD1306_WHITE);
    // Play Boot Sound
    tone(PIEZO_PIN, 3000, 1);
    delay(1);
    tone(PIEZO_PIN, 3000, 50);
    delay(50);
    tone(PIEZO_PIN, 1200, 200);
    delay(200);
    // Draw Boot Animation (Void Glyph Rotating on Z Axis)
    for (int j = 0; j <= 1; j++) {
        for (int i = 0; i <= 15; i++) {
            display.fillRect((OLED_WIDTH / 2) - (MICROWAVE_FRAME_WIDTH / 2) +
                                 (VOID_GLY_FRAME_WIDTH / 4),
                             (OLED_HEIGHT / 2) - (MICROWAVE_FRAME_HEIGHT / 2) +
                                 (VOID_GLY_FRAME_HEIGHT / 5),
                             VOID_GLY_FRAME_WIDTH, VOID_GLY_FRAME_HEIGHT,
                             SSD1306_BLACK);
            display.drawBitmap((OLED_WIDTH / 2) - (MICROWAVE_FRAME_WIDTH / 2) +
                                   (VOID_GLY_FRAME_WIDTH / 4),
                               (OLED_HEIGHT / 2) -
                                   (MICROWAVE_FRAME_HEIGHT / 2) +
                                   (VOID_GLY_FRAME_HEIGHT / 5),
                               void_gly[i], VOID_GLY_FRAME_WIDTH,
                               VOID_GLY_FRAME_HEIGHT, SSD1306_WHITE);
            display.display();
            delay(10);
        }
        // Figured that code to horizonally flip bitmap was too complicated,
        // just sacrificed PROGMEM for a flipped bitmap set.
        for (int i = 15; i >= 0; i--) {
            display.fillRect((OLED_WIDTH / 2) - (MICROWAVE_FRAME_WIDTH / 2) +
                                 (VOID_GLY_FRAME_WIDTH / 4),
                             (OLED_HEIGHT / 2) - (MICROWAVE_FRAME_HEIGHT / 2) +
                                 (VOID_GLY_FRAME_HEIGHT / 5),
                             VOID_GLY_FRAME_WIDTH, VOID_GLY_FRAME_HEIGHT,
                             SSD1306_BLACK);
            display.drawBitmap((OLED_WIDTH / 2) - (MICROWAVE_FRAME_WIDTH / 2) +
                                   (VOID_GLY_FRAME_WIDTH / 4),
                               (OLED_HEIGHT / 2) -
                                   (MICROWAVE_FRAME_HEIGHT / 2) +
                                   (VOID_GLY_FRAME_HEIGHT / 5),
                               void_gly[i], VOID_GLY_FRAME_WIDTH,
                               VOID_GLY_FRAME_HEIGHT, SSD1306_WHITE);
            display.display();
            delay(10);
        }
    }
    delay(1000);
    tone(PIEZO_PIN, 3000, 1);
    delay(1);
    tone(PIEZO_PIN, 3000, 50);
    delay(50);
    tone(PIEZO_PIN, 1200, 200);
    delay(200);
    display.clearDisplay();
    display.display();
    SerialUSB.println(F("Starting main loop..."));
    kpd.begin();
    tBlink = millis();
    iBlink = 1000;
    iPower = 10;
    bDuty = true;
}

// Draws Time centered on display.
void drawCentreString(const String &buf, int x = 0, int y = 0) {
    display.setCursor(64 + (0 - 102 / 2), (64 / 2) + (25 / 2));
    display.print(buf);
}

void loop() {
    // Door Pin Logic
    if (digitalRead(DOOR_PIN) == HIGH) {
        if (bCook) {
            bPause = true;
        }
        digitalWrite(CIRCUIT_A_PIN, HIGH);
        digitalWrite(CIRCUIT_B_PIN, LOW);
        bDoor = false;
    } else {
        if (!bDoor) {
            bDoor = true;
            digitalWrite(CIRCUIT_A_PIN, HIGH);
            digitalWrite(CIRCUIT_B_PIN, HIGH);
        }
    }
    kpd.tick();
    while (kpd.available()) {
        keypadEvent e = kpd.read();
        if (e.bit.EVENT == KEY_JUST_PRESSED) {
            tone(PIEZO_PIN, 784, 125);
            if (e.bit.KEY == '1') {  // 1: Start
                if (digitalRead(DOOR_PIN) == HIGH) {
                    tone(PIEZO_PIN, 384, 125);
                } else {
                    if (!bPause && tCook >= tNone) {
                        tCook = tCook + tStep30;
                    }
                    bStart = true;
                    iBlink = 500;
                    iCook = millis();
                    iDuty = millis();
                    bCook = true;
                    bPause = false;
                    if (!bStarted) {
                        iDone = millis();
                        iTones = 0;
                        iWait = 325;
                    }
                    bStarted = true;
                    digitalWrite(CIRCUIT_A_PIN, LOW);
                    digitalWrite(CIRCUIT_B_PIN, LOW);
                }
            } else if (e.bit.KEY == '3') {  // 3: Add 10Sec
                tone(PIEZO_PIN, 784, 125);
                tCook = tCook + tStep10;
                if (!bCook) {
                    iBlink = 500;
                    bCook = true;
                    bPause = true;
                }
            } else if (e.bit.KEY == '2') {  // 2: Dec 10Sec
                if (tCook > tNone) {
                    tone(PIEZO_PIN, 784, 125);
                    if (tCook.secondstime() > 10) {
                        tCook = tCook - tStep10;
                    } else {
                        tCook = tNone;
                    }
                    if (tCook == tNone) {
                        if (bCook) {
                            bDone = true;
                            iDone = millis();
                            iTones = 0;
                            bStop = true;
                            tCook = tNone;
                            iWait = 325;
                        }
                        bPause = false;
                        iBlink = 1000;
                        digitalWrite(CIRCUIT_A_PIN, HIGH);
                        if (digitalRead(DOOR_PIN) == HIGH) {
                            digitalWrite(CIRCUIT_B_PIN, LOW);
                        } else {
                            digitalWrite(CIRCUIT_B_PIN, HIGH);
                        }
                    }
                } else {
                    tone(PIEZO_PIN, 384, 125);
                }
            } else if (e.bit.KEY == '4') {  // 4: Pause-Stop
                tone(PIEZO_PIN, 784, 125);
                if (bStarted && !bPause && tCook > tNone) {
                    bPause = true;
                    digitalWrite(CIRCUIT_A_PIN, HIGH);
                    digitalWrite(CIRCUIT_B_PIN, HIGH);
                } else {
                    tCook = tNone;
                    iBlink = 1000;
                    bStop = true;
                    bDone = true;
                    iDone = millis();
                    iTones = 0;
                    iWait = 325;
                    digitalWrite(CIRCUIT_A_PIN, HIGH);
                    if (digitalRead(DOOR_PIN) == HIGH) {
                        digitalWrite(CIRCUIT_B_PIN, LOW);
                    } else {
                        digitalWrite(CIRCUIT_B_PIN, HIGH);
                    }
                }
            }
        }
    }
    DateTime tNow = myTZ.toLocal(now(), &tcr);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setFont(&consola20pt7b);
    if (millis() >= tBlink + iBlink) {
        tBlink = millis();
        if (bBlink) {
            bBlink = false;
        } else {
            bBlink = true;
        }
    }
    if (bStarted && !bDone && bStart) {
        if (millis() - iDone >= iWait) {
            bStart = false;
        } else {
            if (iTones < 5) {
                if (millis() - iTone >= 50) {
                    iTone = millis();
                    tone(PIEZO_PIN, 784, 50);
                    iTones++;
                    if (iTones == 5) {
                        tone(PIEZO_PIN, 1084, 125);
                    }
                }
            }
        }
    }
    if (bCook) {
        sprintf(sDisplay,
                sFormat[iFormat + (bPause ? 1 : 0) + (bBlink ? 0 : 1)],
                tCook.minute(), tCook.second());
        if (bStarted && !bDone && !bBlink && !bPause &&
            millis() >= iCook + 1000) {
            if (iPower < 10 && iPower > 0) {
                if (millis() - iDuty >= 10000) {
                    iDuty = millis();
                    digitalWrite(CIRCUIT_A_PIN, LOW);
                    digitalWrite(CIRCUIT_B_PIN, LOW);
                }
                if (millis() - iDuty >= iPower * 1000) {
                    digitalWrite(CIRCUIT_A_PIN, HIGH);
                    digitalWrite(CIRCUIT_B_PIN, LOW);
                }
            }
            iCook = millis();
            if (tCook > tNone) {
                tCook = tCook - tDec;
            }
            if (tCook == tNone) {
                bDone = true;
                iDone = millis();
                iTones = 0;
                iWait = 5500;
            }
        }
        if (bDone) {
            if (bStarted) {
                digitalWrite(CIRCUIT_A_PIN, HIGH);
                digitalWrite(CIRCUIT_B_PIN, LOW);
            }
            if (millis() - iDone >= iWait) {
                digitalWrite(CIRCUIT_A_PIN, HIGH);
                if (digitalRead(DOOR_PIN) == HIGH) {
                    digitalWrite(CIRCUIT_B_PIN, LOW);
                } else {
                    digitalWrite(CIRCUIT_B_PIN, HIGH);
                }
                bCook = false;
                bDone = false;
                bStop = false;
                bStarted = false;
                bPause = false;
            } else {
                if (bStop) {
                    if (iTones < 5) {
                        if (millis() - iTone >= 50) {
                            iTone = millis();
                            tone(PIEZO_PIN, 584, 50);
                            iTones++;
                            if (iTones == 5) {
                                tone(PIEZO_PIN, 384, 125);
                            }
                        }
                    }
                } else {
                    if (iTones < 5) {
                        if (millis() - iTone >= 125) {
                            iTone = millis();
                            tone(PIEZO_PIN, 784, 125);
                            iTones++;
                            if (iTones == 5) {
                                tone(PIEZO_PIN, 784, 500);
                            }
                        }
                    } else if (iTones <= 10) {
                        if (millis() - iTone >= 750) {
                            iTone = millis();
                            tone(PIEZO_PIN, 784, 500);
                            iTones++;
                        }
                    }
                }
            }
        }
    } else {
        iFormat = 0;
        sprintf(sDisplay, sFormat[iFormat + (bBlink ? 0 : 1)],
                tNow.twelveHour(), tNow.minute());
    }
    drawCentreString(sDisplay);
    display.display();
}