# mWaveCotnrol
 Microwave Oven Controller

```
The keypad on this little microwave went out for some reason, and indtead of
buying another one I descided to use some hardware laying around and make a
basic controller as a replacment.

I started to examine the circuitry in the microwave to understand how
everything functioned.

I started with an ESP32 running micropython, a rotary encoder and a 128x64
OLED display for the UI, and an RTC so it didnt loose the time constantly.
Later I wanted the esp32 for another experiment so I ported the code to
Arduino.

At this point everything was on a breadboard and humidity from the microwave
not having the panel on kept causing issues with the breadboard. That and
boiling over a deep fryer didnt help.

Descided to make the project internal, drilled a hole in the front face of
the microwave for the encoder and I thought it would be a good idea to use my
impact drill to tighten the nut, well that broke the encoder. Went and
upgraded to a 1x4 membrane keypad.

I packed everything nice and neat in the cabinet and ran a usb cable out of
the back.
```