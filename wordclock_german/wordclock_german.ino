/*
 * Sourcecode for wordclock (german)
 * 
 * created by techniccontroller 06.12.2020
 * 
 * changelog:
 * 03.04.2021: add DCF77 signal quality check
 * 04.04.2021: add update intervall for RTC update
 */
#include "RTClib.h"             //https://github.com/adafruit/RTClib
#include "DCF77.h"              //https://github.com/thijse/Arduino-DCF77 
#include <Time.h>               //https://github.com/PaulStoffregen/Time
#include <TimeLib.h>
#include <Adafruit_GFX.h>       //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_NeoMatrix.h> //https://github.com/adafruit/Adafruit_NeoMatrix
#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel
#include <EEPROM.h>


// definition of pins
#define DCF_PIN 2               // Connection pin to DCF 77 device
#define DCF_INTERRUPT 0         // Interrupt number associated with pin
#define NEOPIXEL_PIN 6          // Connection pin to Neopixel LED strip
int sensorPin = A6;             // analog input pin for light sensor


// char array to save time an date as string
char time_s[9];
char date_s[11];

// define parameters for the project
const int width = 11;           // width of LED matirx
const int height = 10+1;        // height of LED matrix + additional row for minute leds
const int eeAddressTime = 10;   // eeprom address for time value (persist values during power off)
const int eeAddressColor = 20;  // eeprom address for color value (persist values during power off)
const int upperLight = 930;     // upper threshold for lightsensor (above this value brightness is always 20%)
const int lowerLight = 830;     // lower threshold for lightsensor (below this value brightness is always 100%)
const int centerrow = height/2; // id of center row
const int centercol = width/2;  // id of center column

// create DCF77 object  
DCF77 DCF = DCF77(DCF_PIN,DCF_INTERRUPT);

// create RTC object
RTC_DS3231 rtc;

// define mapping array for nicer printouts
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// create Adafruit_NeoMatrix object
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(width, height, NEOPIXEL_PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS    + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

// define color modes
const uint16_t colors[] = {
  matrix.Color(255, 255, 255),  // white (in this mode together with colorwheel)
  matrix.Color(255, 0, 0),      // red
  matrix.Color(255, 255, 0),    // yellow
  matrix.Color(255, 0, 200),    // magenta
  matrix.Color(128, 128, 128),  // white (darker)
  matrix.Color(0, 255, 0),      // green 
  matrix.Color(0, 0, 255) };    // blue

// definition of global variables
int valLight = 100;             // currentvalue of light sensor
int brightness = 20;            // current brughtness for leds
int activeColorID = 0;          // current active color mode
int offset = 0;                 // offset for colorwheel
long updateIntervall = 120000;       // Updateintervall 2 Minuten
long updateTimer = 0;           // Zwischenspeicher für die Wartezeit

// representation of matrix as 2D array
int grid[height][width] = {{0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0,0,0,0}};

// function prototypes
void timeToArray(uint8_t hours, uint8_t minutes);
void printDateTime(DateTime datetime);
void checkDCFSignal();
int DCF77signalQuality(int pulses);
void checkForNewDCFTime(DateTime now);
void gridAddPixel(uint8_t x, uint8_t y);
void gridFlush(void);
void drawOnMatrix(void);
void timeToArray(uint8_t hours, uint8_t minutes);

void setup() {
  // enable serial output
  Serial.begin(9600);

  // Measure DCF signal quality
  //checkDCFSignal();

  // Init DCF
  DCF.Start();

  // Init RTC
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  // get active color from last power cycle
  EEPROM.get(eeAddressColor, activeColorID);
  
  // check if in the last 3 seconds was a power cycle
  // if yes, so change to next color mode
  long laststartseconds = 0;
  EEPROM.get(eeAddressTime, laststartseconds);
  long currentseconds = rtc.now().secondstime();
  Serial.print("Startseconds: ");
  Serial.println(laststartseconds);
  Serial.print("Currentseconds: ");
  Serial.println(currentseconds);
  if(currentseconds - laststartseconds < 10){
    activeColorID = (activeColorID+1)%7;
    Serial.print("change color to ");
    Serial.println(activeColorID);
    EEPROM.put(eeAddressColor, activeColorID);
  }
  EEPROM.put(eeAddressTime, currentseconds);
  Serial.print("active color: ");
  Serial.println(activeColorID);

  Serial.println("Buildtime: ");
  Serial.println(F(__DATE__));
  Serial.println(F(__TIME__));

  // check if RTC battery was changed
  if (rtc.lostPower() || DateTime(F(__DATE__), F(__TIME__)) > rtc.now()) {
    Serial.println("RTC lost power or RTC time is behind build time, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Init LED matrix
  matrix.begin();
  matrix.setBrightness(100);
}

void loop() {
  // get light value from light sensor
  valLight = analogRead(sensorPin);

  // calc led brightness from light value
  if (valLight < lowerLight){
    brightness = 100;
  }
  else if (valLight > upperLight){
    brightness = 20;
  }
  else{
    brightness = 80-((valLight-lowerLight)*1.0/(upperLight-lowerLight))*80 + 20;
  }

  Serial.print("Light: ");
  Serial.println(valLight);
  Serial.print("Brightness: ");
  Serial.println(brightness);
  matrix.setBrightness(brightness);

  
  // Print current date and time
  DateTime rtctime = rtc.now();
  Serial.print("RTC: ");
  printDateTime(rtctime);

  Serial.print("Temperature: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");

  // check for a new time on the DCF receiver
  checkForNewDCFTime(rtctime);

  // convert time to a grid representation
  timeToArray(rtctime.hour(), rtctime.minute());
  // clear matrix
  matrix.fillScreen(0);

  // add condition if nightmode (LEDs = OFF) should be activated
  if(rtctime.hour() > 22 && rtctime.hour() < 6){ // turn in off LEDs between 22:00 and 6:00
    // do nothing, matrix already cleared (see previous statment)
  } else {
    // do the normal job and display the time on the wordclock

    // if color mode is set to 0, a color wheel will be shown in background
    if(activeColorID == 0){
      // draw colorwheel on matrix
      drawCircleOnMatrix(offset);
      offset = (offset + 1)%256;
    }

    // draw grid reprentation of time to matrix
    drawOnMatrix();
  }
  // send the commands to the LEDs
  matrix.show();

  // change depending on color mode the refreshing time of clock
  if(activeColorID == 0){
    delay(500);
  } else {
    delay(5000);
  }
  
}

// Draws a 360 degree colorwheel on the matrix rotated by offset
void drawCircleOnMatrix(int offset){
  for(int r = 0; r < height; r++){
    for(int c = 0; c < width; c ++){
      int angle = ((int)((atan2(r - centerrow, c - centercol) * (180/M_PI)) + 180) % 360);
      int hue =  (int)(angle * 255.0/360 + offset) % 256;
      matrix.drawPixel(c,r, Wheel(hue));
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return matrix.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return matrix.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return matrix.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// Prints given date and time to serial
void printDateTime(DateTime datetime){
  Serial.print(datetime.year(), DEC);
  Serial.print('/');
  Serial.print(datetime.month(), DEC);
  Serial.print('/');
  Serial.print(datetime.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[datetime.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(datetime.hour(), DEC);
  Serial.print(':');
  Serial.print(datetime.minute(), DEC);
  Serial.print(':');
  Serial.print(datetime.second(), DEC);
  Serial.println();
}

void checkDCFSignal(){
  pinMode(DCF_PIN, INPUT);
  Serial.println("Measure signal quality DCF77 (please wait)");
  Serial.println("NO SIGNAL   <- |                                          <- MISERABLE <- |  BAD      <- |          GOOD         | -> BAD       | -> MISERABLE ->");

  //Do measurement over 10 impules, one impulse takes exactly one Second
  int q = DCF77signalQuality(10);
  //If no change between HIGH and LOW was detected at the connection, 
  //this means in 99.99% of all cases that the DCF receiver does not work 
  //because with extremely poor reception you have changes, but you cannot evaluate them. 
  if (!q) {Serial.print("# (Check connection!)");}
  for (int i = 0; i < q; i++) {
    Serial.print(">");
  }
  Serial.println("");
  
}

// check signalquality of DCF77 receiver (code from Ralf Bohnen, 2013
int DCF77signalQuality(int pulses) {
  int prevSensorValue=0;
  unsigned long loopTime = 10000; //Impuls Länge genau eine Sekunde
  //Da wir ja mitten in einem Impuls einsteigen könnten, verwerfen wir den ersten.
  int rounds = -1; 
  unsigned long gagingStart = 0;
  unsigned long waitingPeriod = 0;
  int overallChange = 0;
  int change = 0;

  while (true) {
    //Unsere Schleife soll das Eingangssignal (LOW oder HIGH) 10 mal pro
    //Sekunde messen um das sicherzustellen, messen wir dessen Ausführungszeit.
    gagingStart = micros();
    int sensorValue = digitalRead(DCF_PIN);
    //Wenn von LOW nach HIGH gewechselt wird beginnt ein neuer Impuls
    if (sensorValue==1 && prevSensorValue==0) { 
      rounds++;
      if (rounds > 0 && rounds < pulses + 1) {overallChange+= change;}
      if (rounds == pulses) { return overallChange /pulses;}
      change = 0; 
    }
    prevSensorValue = sensorValue;
    change++;

    //Ein Wechsel zwichen LOW und HIGH müsste genau alle 100 Durchläufe stattfinden
    //wird er größer haben wir kein Empfang
    //300 habe ich als guten Wert ermittelt, ein höherer Wert würde die Aussage festigen
    //erhöht dann aber die Zeit.
    if (change > 300) {return 0;}
    //Berechnen und anpassen der Ausführungszeit
    waitingPeriod = loopTime - (micros() - gagingStart);
    delayMicroseconds(waitingPeriod);
  }
}

// Checks for new time from DCF77 and updates RTC time
void checkForNewDCFTime(DateTime now){
  // check if new time from DCF77 is available
  time_t DCFtime = DCF.getTime(); 
  if (DCFtime!=0)
  {
    setTime(DCFtime);
    // print new time from DCF77
    DateTime currentDCFtime = DateTime(year(), month(), day(), hour(), minute(), second());
    Serial.print("Got new time from DCF:");
    printDateTime(currentDCFtime);
    /*snprintf(time_s,sizeof(time_s),"%.2d:%.2d:%.2d" , hour(), minute(), second());
    time_s[strlen(time_s)] = '\0';
    snprintf(date_s,sizeof(date_s),"%.2d.%.2d.%.4d" , day(), month(), year());
    date_s[strlen(date_s)] = '\0';
    Serial.print(time_s); 
    Serial.print("  "); 
    Serial.println(date_s);*/

    // calc the difference to current time of RTC
    //int diff = abs(currentDCFtime.minute() - now.minute());
    //int norm_diff = diff > 30 ? abs(diff-60): diff;
    //Serial.print("Current diff: ");
    //Serial.print(norm_diff);
    //Serial.println(" minutes");

    // id update intervall is over update RTC time with DCF time
    if((millis() - updateTimer) > updateIntervall){
      Serial.println("Adjust RTC time");
      rtc.adjust(currentDCFtime);
      updateTimer = millis();
    }
  }
}

// "activates" a pixel in grid
void gridAddPixel(uint8_t x,uint8_t y){
    grid[y][x] = 1;
}

// "deactivates" all pixels in grid
void gridFlush(void){
    //Setzt an jeder Position eine 0
    for(uint8_t i=0; i<height; i++){
        for(uint8_t j=0; j<width; j++){
            grid[i][j] = 0;
        }
    }
}

// draws the grid to the ledmatrix with the current active color
void drawOnMatrix(){
  for(int s = 0; s < width; s++){
    for(int z = 0; z < height; z++){
      if(grid[z][s] != 0){
        Serial.print("1 ");
        matrix.drawPixel(s,z,colors[activeColorID]); 
      }
      else{
        Serial.print("0 ");
      }
    }
    Serial.println();  
  }
}

// Converts the given time in grid representation
void timeToArray(uint8_t hours,uint8_t minutes){
  Serial.println(hours);
  Serial.println(minutes);
  
  //clear grid
  gridFlush();
  
  //start filling grid with pixels
  
  //ES IST
  Serial.print("ES IST ");
  gridAddPixel(0,0);
  gridAddPixel(1,0);
  gridAddPixel(3,0);
  gridAddPixel(4,0);
  gridAddPixel(5,0);
  
  //show minutes
  if(minutes >= 5 && minutes < 10)
  {
      //Fünf
      Serial.print("FÜNF ");
      gridAddPixel(7,0);
      gridAddPixel(8,0);
      gridAddPixel(9,0);
      gridAddPixel(10,0);
      //Nach
      Serial.print("NACH ");
      gridAddPixel(7,3);
      gridAddPixel(8,3);
      gridAddPixel(9,3);
      gridAddPixel(10,3);
  }
  else if(minutes >= 10 && minutes < 15)
  {
      //Zehn
      Serial.print("ZEHN ");
      gridAddPixel(0,1);
      gridAddPixel(1,1);
      gridAddPixel(2,1);
      gridAddPixel(3,1);
      //Nach
      Serial.print("NACH ");
      gridAddPixel(7,3);
      gridAddPixel(8,3);
      gridAddPixel(9,3);
      gridAddPixel(10,3);;
  }
  else if(minutes >= 15 && minutes < 20)
  {
      //Viertel
      Serial.print("VIERTEL ");
      gridAddPixel(4,2);
      gridAddPixel(5,2);
      gridAddPixel(6,2);
      gridAddPixel(7,2);
      gridAddPixel(8,2);
      gridAddPixel(9,2);
      gridAddPixel(10,2);
      //Nach
      Serial.print("NACH ");
      gridAddPixel(7,3);
      gridAddPixel(8,3);
      gridAddPixel(9,3);
      gridAddPixel(10,3);
  }
  else if(minutes >= 20 && minutes < 25)
  {
  
      //Zehn
      Serial.print("ZEHN ");
      gridAddPixel(0,1);
      gridAddPixel(1,1);
      gridAddPixel(2,1);
      gridAddPixel(3,1);
      //Vor
      Serial.print("VOR ");
      gridAddPixel(0,3);
      gridAddPixel(1,3);
      gridAddPixel(2,3);
      //Halb
      Serial.print("HALB ");
      gridAddPixel(0,4);
      gridAddPixel(1,4);
      gridAddPixel(2,4);
      gridAddPixel(3,4);
  
  }
  else if(minutes >= 25 && minutes < 30)
  {
      //Fünf
      Serial.print("FÜNF ");
      gridAddPixel(7,0);
      gridAddPixel(8,0);
      gridAddPixel(9,0);
      gridAddPixel(10,0);
      //Vor
      Serial.print("VOR ");
      gridAddPixel(0,3);
      gridAddPixel(1,3);
      gridAddPixel(2,3);
      //Halb
      Serial.print("HALB ");
      gridAddPixel(0,4);
      gridAddPixel(1,4);
      gridAddPixel(2,4);
      gridAddPixel(3,4);
  }
  else if(minutes >= 30 && minutes < 35)
  {
      //Halb
      Serial.print("HALB ");
      gridAddPixel(0,4);
      gridAddPixel(1,4);
      gridAddPixel(2,4);
      gridAddPixel(3,4);
  }
  else if(minutes >= 35 && minutes < 40)
  {
      //Fünf
      Serial.print("FÜNF ");
      gridAddPixel(7,0);
      gridAddPixel(8,0);
      gridAddPixel(9,0);
      gridAddPixel(10,0);
      //Nach
      Serial.print("NACH ");
      gridAddPixel(7,3);
      gridAddPixel(8,3);
      gridAddPixel(9,3);
      gridAddPixel(10,3);
      //Halb
      Serial.print("HALB ");
      gridAddPixel(0,4);
      gridAddPixel(1,4);
      gridAddPixel(2,4);
      gridAddPixel(3,4);
  }
  else if(minutes >= 40 && minutes < 45)
  {
      //Zehn
      Serial.print("ZEHN ");
      gridAddPixel(0,1);
      gridAddPixel(1,1);
      gridAddPixel(2,1);
      gridAddPixel(3,1);
      //Nach
      Serial.print("NACH ");
      gridAddPixel(7,3);
      gridAddPixel(8,3);
      gridAddPixel(9,3);
      gridAddPixel(10,3);
      //Halb
      Serial.print("HALB ");
      gridAddPixel(0,4);
      gridAddPixel(1,4);
      gridAddPixel(2,4);
      gridAddPixel(3,4);
  }
  else if(minutes >= 45 && minutes < 50)
  {
      //Viertel
      Serial.print("VIERTEL ");
      gridAddPixel(4,2);
      gridAddPixel(5,2);
      gridAddPixel(6,2);
      gridAddPixel(7,2);
      gridAddPixel(8,2);
      gridAddPixel(9,2);
      gridAddPixel(10,2);
      //Vor
      Serial.print("VOR ");
      gridAddPixel(0,3);
      gridAddPixel(1,3);
      gridAddPixel(2,3);
  }
  else if(minutes >= 50 && minutes < 55)
  {
      //Zehn
      Serial.print("ZEHN ");
      gridAddPixel(0,1);
      gridAddPixel(1,1);
      gridAddPixel(2,1);
      gridAddPixel(3,1);
      //Vor
      Serial.print("VOR ");
      gridAddPixel(0,3);
      gridAddPixel(1,3);
      gridAddPixel(2,3);
  
  }
  else if(minutes >= 55 && minutes < 60)
  {
  
      //Fünf
      Serial.print("FÜNF ");
      gridAddPixel(7,0);
      gridAddPixel(8,0);
      gridAddPixel(9,0);
      gridAddPixel(10,0);
      //Vor
      Serial.print("VOR ");
      gridAddPixel(0,3);
      gridAddPixel(1,3);
      gridAddPixel(2,3);
  }

  //separate LEDs for minutes in an additional row
  {
  switch (minutes%5)
        { 
          case 0:
            break;
              
          case 1:
            gridAddPixel(0,10);
            break;

          case 2:
            gridAddPixel(0,10);
            gridAddPixel(1,10);
            break;

          case 3:
            gridAddPixel(0,10);
            gridAddPixel(1,10);
            gridAddPixel(2,10);
            break;

          case 4:
            gridAddPixel(0,10);
            gridAddPixel(1,10);
            gridAddPixel(2,10);
            gridAddPixel(3,10);
            break;
        }
  }

  //convert hours to 12h format
  if(hours >= 12)
  {
      hours -= 12;
  }
  if(minutes >= 20)
  {
      hours++;
  }
  if(hours == 12)
  {
      hours = 0;
  }

  // show hours
  switch(hours)
  {
  case 0:
      //Zwölf
      Serial.print("ZWÖLF ");
      gridAddPixel(6,8);
      gridAddPixel(7,8);
      gridAddPixel(8,8);
      gridAddPixel(9,8);
      gridAddPixel(10,8);
      break;
  case 1:
      //EIN(S)
      Serial.print("EIN");
      gridAddPixel(0,5);
      gridAddPixel(1,5);
      gridAddPixel(2,5);
      if(minutes > 4){
        gridAddPixel(3,5);
      }
      break;
  case 2:
      //Zwei
      Serial.print("ZWEI ");
      gridAddPixel(7,5);
      gridAddPixel(8,5);
      gridAddPixel(9,5);
      gridAddPixel(10,5);
      break;
  case 3:
      //Drei
      Serial.print("DREI ");
      gridAddPixel(0,6);
      gridAddPixel(1,6);
      gridAddPixel(2,6);
      gridAddPixel(3,6);
      break;
  case 4:
      //Vier
      Serial.print("VIER ");
      gridAddPixel(7,6);
      gridAddPixel(8,6);
      gridAddPixel(9,6);
      gridAddPixel(10,6);
      break;
  case 5:
      //Fünf
      Serial.print("FÜNF ");
      gridAddPixel(7,4);
      gridAddPixel(8,4);
      gridAddPixel(9,4);
      gridAddPixel(10,4);
      break;
  case 6:
      //Sechs
      Serial.print("SECHS ");
      gridAddPixel(0,7);
      gridAddPixel(1,7);
      gridAddPixel(2,7);
      gridAddPixel(3,7);
      gridAddPixel(4,7);
      break;
  case 7:
      //Sieben
      Serial.print("SIEBEN ");
      gridAddPixel(0,8);
      gridAddPixel(1,8);
      gridAddPixel(2,8);
      gridAddPixel(3,8);
      gridAddPixel(4,8);
      gridAddPixel(5,8);
      break;
  case 8:
      //Acht
      Serial.print("ACHT ");
      gridAddPixel(7,7);
      gridAddPixel(8,7);
      gridAddPixel(9,7);
      gridAddPixel(10,7);
      break;
  case 9:
      //Neun
      Serial.print("NEUN ");
      gridAddPixel(3,9);
      gridAddPixel(4,9);
      gridAddPixel(5,9);
      gridAddPixel(6,9);
      break;
  case 10:
      //Zehn
      Serial.print("ZEHN ");
      gridAddPixel(0,9);
      gridAddPixel(1,9);
      gridAddPixel(2,9);
      gridAddPixel(3,9);
      break;
  case 11:
      //Elf
      Serial.print("ELF ");
      gridAddPixel(5,4);
      gridAddPixel(6,4);
      gridAddPixel(7,4);
      break;
  }
  if(minutes < 5)
  {
      //UHR
      Serial.print("UHR ");
      gridAddPixel(8,9);
      gridAddPixel(9,9);
      gridAddPixel(10,9);
  }
  
  Serial.println();
}
