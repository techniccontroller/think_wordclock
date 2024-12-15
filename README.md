# Wordclock with Arduino and NeoPixel
Arduino source code for my DIY Wordclock.

<img src="https://techniccontroller.com/wp-content/uploads/1060178-938x1024.jpg" width="400px" title="Wordclock"/>



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

https://techniccontroller.com/word-clock-with-arduino-and-neopixel/

## Installation

1. Download the code (ZIP) and unzip it, copy the complete unzipped folder into your sketchbock path (found in ArduinoIDE under File->Settings).
2. Start Arduino IDE and open the file `wordclock_german.ino` via **File -> Open ...**.
3. Now the libraries must be installed. To avoid complications, I recommend simply copying all folders from the `libraries` project folder into the `libraries` folder of the sketchbook path. You can also install the libraries manually via the library manager of the IDE, but the time library of PaulStoffregen is missing, you still have to copy it yourself.

    Your folder structure should look like this:
    ```
    ./MySketchbookLocation 
    │
    └───libraries
    │   └───Adafruit_BusIO
    │   └───Adafruit_GFX_Library
    │   └───Adafruit_NeoMatrix
    │   └───Adafruit_NeoPixel
    │   └───DCF77
    |   └───RTClib
    |   └───Time
    │   
    └───think_wordclock
        └───libraries
        │   └───Adafruit_BusIO
        │   └───Adafruit_GFX_Library
        │   └───Adafruit_NeoMatrix
        │   └───Adafruit_NeoPixel
        │   └───DCF77
        |   └───RTClib
        |   └───Time
        │   
        └───wordclock_german
        │   └───wordclock_german.ino
        │
        └───pictures
        |   └───(...)
        └───README.md 
    ```

4. Connect your Arduino Nano to your computer.
5. In Arduino IDE select the correct board (Arduino Nano) and serial port.
6. Compile and upload code via **Sketch -> Upload**.


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



