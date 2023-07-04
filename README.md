# Think Wordclock
Arduino source code for my DIY Wordclock.

<img src="https://techniccontroller.de/wp-content/uploads/1060178-938x1024.jpg" width="400px" title="Wordclock"/>



## Features
This project includes the following features:
- Arduino Nano as a microcontroller
- 114 NeoPixel LEDs WS2812b as Zic-Zac matrix starting from top left  
- RTC module DS3231
- DCF77 receiver for German clock signal
- 7 different color modes (switched when turned off/on within 10 seconds after startup)
- light sensor to adjust the brightness of LEDs to the environment
- DCF77 signal quality measurement on every startup and display on LED matrix (see description below)

## Full Tutorial
The full tutorial on how to build this Wordclock is published on 

https://think-thi.de/2020/12/08/diy-wortuhr-mit-arduino-und-neopixel/

https://techniccontroller.com/word-clock-with-arduino-and-neopixel/

## Used Libraries
To use this source code, you need to install the following libraries in Arduino IDE.
I have also included the versions of the libraries I use in this repository. 
In case of any problems, please use the versions I've provided.

- https://github.com/adafruit/RTClib
- https://github.com/PaulStoffregen/Time
- https://github.com/thijse/Arduino-DCF77
- https://github.com/adafruit/Adafruit-GFX-Library
- https://github.com/adafruit/Adafruit_NeoPixel
- https://github.com/adafruit/Adafruit_NeoMatrix
- https://github.com/adafruit/Adafruit_BusIO

## DCF77 Signal quality measurement

I added a signal measurement feature for the DCF77 receiver. The signal quality is now also displayed at each startup with the LEDs:
1. (during measurement) top left LED flashes blue, 
2. (after measurement) red/green the signal quality is shown as a bar for 2 seconds (10 leds = good, 1 led = bad)


https://user-images.githubusercontent.com/36072504/212412823-a1ee02de-0112-4e50-9674-2f69df8ed7c8.mp4



