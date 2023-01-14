/*
 * Sourcecode for wordclock (german)
 * 
 * created by techniccontroller 06.12.2020
 * 
 * changelog:
 * 03.04.2021: add DCF77 signal quality check
 * 04.04.2021: add update intervall for RTC update
 * 18.10.2021: add nightmode
 * 17.04.2022: fix nightmode condition, clean serial output, memory reduction
 * 13.05.2022: add PIR sensor to interrupt nightmode while PIR_PIN == HIGH (FEATURE-REQUEST)
 * 13.01.2023: refactoring of code to reduce memory usage (e.g. reduce length of strings in prints)
 *             and add DCF signal quality check on every startup and display result
 */
#include "RTClib.h"             //https://github.com/adafruit/RTClib
#include "DCF77.h"              //https://github.com/thijse/Arduino-DCF77                
#include <TimeLib.h>            //https://github.com/PaulStoffregen/Time
#include <Adafruit_GFX.h>       //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_NeoMatrix.h> //https://github.com/adafruit/Adafruit_NeoMatrix
#include <EEPROM.h>


// definition of pins
#define DCF_PIN 2               // Connection pin to DCF 77 device
#define NEOPIXEL_PIN 6          // Connection pin to Neopixel LED strip
#define SENSOR_PIN A6           // analog input pin for light sensor
#define PIR_PIN 4               // Connection pin to PIR device (HC-SR501, Jumper on H = Repeatable Trigger)

// char array to save time an date as string
char time_s[9];
char date_s[11];

// define parameters for the project
#define WIDTH 11                // width of LED matirx
#define HEIGHT (10+1)           // height of LED matrix + additional row for minute leds
#define EE_ADDRESS_TIME 10      // eeprom address for time value (persist values during power off)
#define EE_ADDRESS_COLOR 20     // eeprom address for color value (persist values during power off)
#define UPPER_LIGHT_THRSH 930   // upper threshold for lightsensor (above this value brightness is always 20%)
#define LOWER_LIGHT_THRSH 800   // lower threshold for lightsensor (below this value brightness is always 100%)
#define CENTER_ROW (HEIGHT/2)   // id of center row
#define CENTER_COL (WIDTH/2)    // id of center column
#define NIGHTMODE_START 22      // start hour of nightmode (22 <=> 22:00)
#define NIGHTMODE_END 6         // end hour of nightmode (6 <=> 6:00)

// create DCF77 object  
DCF77 DCF = DCF77(DCF_PIN,digitalPinToInterrupt(DCF_PIN));

// create RTC object
RTC_DS3231 rtc;

// define mapping array for nicer printouts
char daysOfTheWeek[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// create Adafruit_NeoMatrix object
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(WIDTH, HEIGHT, NEOPIXEL_PIN,
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
uint8_t brightness = 20;        // current brughtness for leds
uint8_t activeColorID = 0;      // current active color mode
int offset = 0;                 // offset for colorwheel
long updateIntervall = 120000;  // Updateintervall 2 Minuten
long updateTimer = 0;           // Zwischenspeicher für die Wartezeit

// representation of matrix as byte array (1 led = 1 bit) 
uint8_t grid[HEIGHT*WIDTH/8 + 1] = {0};

// function prototypes
void timeToArray(uint8_t hours, uint8_t minutes);
void printDateTime(DateTime datetime);
int checkDCFSignal();
int DCF77signalQuality(int pulses);
void checkForNewDCFTime(DateTime now);
void gridAddPixel(uint8_t x, uint8_t y);
void gridFlush(void);
void drawOnMatrix(void);
void timeToArray(uint8_t hours, uint8_t minutes);
uint8_t readGrid(uint8_t x, uint8_t y);
void writeGrid(uint8_t x, uint8_t y, uint8_t val);

void setup() {
  // enable serial output
  Serial.begin(9600);
  Serial.println("Buildtime: ");
  Serial.println(F(__DATE__));
  Serial.println(F(__TIME__));

  // Init LED matrix
  matrix.begin();
  matrix.setBrightness(100);

  // Init RTC
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  // get active color from last power cycle
  EEPROM.get(EE_ADDRESS_COLOR, activeColorID);
  
  // check if in the last 20 seconds was a power cycle
  // if yes, so change to next color mode
  // if no, do a normal start including dcf signal quality check and matrix test
  long laststartseconds = 0;
  EEPROM.get(EE_ADDRESS_TIME, laststartseconds);
  long currentseconds = rtc.now().secondstime();
  if(currentseconds - laststartseconds < 20){
    activeColorID = (activeColorID+1)%7;
    Serial.print("change color to ");
    Serial.println(activeColorID);
    EEPROM.put(EE_ADDRESS_COLOR, activeColorID);
  }
  else{

    // measure DCF signal quality
    int dcfquality = checkDCFSignal();
    
    // show DCF quality as a red/green bar
    uint16_t dcfcolor = dcfquality < 5 ? colors[1]: colors[5];
    for(int i=0; i<WIDTH; i++){
      if(i < dcfquality){
        matrix.drawPixel(i, 0, dcfcolor);
      } else {
        matrix.drawPixel(i, 0, matrix.Color(0,0,0));
      }
    }
    matrix.show();
    delay(2000);

    // quick matrix test
    for(int i=1; i<(WIDTH*HEIGHT); i++){
      matrix.drawPixel((i-1)%WIDTH, (i-1)/HEIGHT, matrix.Color(0,0,0));
      matrix.drawPixel(i%WIDTH, i/HEIGHT, matrix.Color(120,150,150));
      matrix.show();
      delay(10);
    }
  }
  EEPROM.put(EE_ADDRESS_TIME, currentseconds);
  Serial.print("active color: ");
  Serial.println(activeColorID);

  // Init DCF
  DCF.Start();

  // check if RTC battery was changed
  if (rtc.lostPower() || DateTime(F(__DATE__), F(__TIME__)) > rtc.now()) {
    // RTC lost power or RTC time is behind build time, let's set the time!
    Serial.println("RTC lost power");
    printDateTime(rtc.now());
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  // get light value from light sensor
  valLight = analogRead(SENSOR_PIN);

  // calc led brightness from light value
  if (valLight < LOWER_LIGHT_THRSH){
    brightness = 100;
  }
  else if (valLight > UPPER_LIGHT_THRSH){
    brightness = 20;
  }
  else{
    brightness = 80-((valLight-LOWER_LIGHT_THRSH)*1.0/(UPPER_LIGHT_THRSH-LOWER_LIGHT_THRSH))*80 + 20;
  }

  Serial.print("Light: ");
  Serial.println(valLight);
  Serial.print("Brightness: ");
  Serial.println(brightness);
  matrix.setBrightness(brightness*2);

  
  // Print current date and time
  DateTime rtctime = rtc.now();
  Serial.print("RTC: ");
  printDateTime(rtctime);

  Serial.print("Temp: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");

  // check for a new time on the DCF receiver
  checkForNewDCFTime(rtctime);

  // convert time to a grid representation
  timeToArray(rtctime.hour(), rtctime.minute());
  // clear matrix
  matrix.fillScreen(matrix.Color(0, 0, 0));

  
  // add condition if nightmode (LEDs = OFF) should be activated
  // turn off LEDs between NIGHTMODE_START and NIGHTMODE_END
  uint8_t nightmode = false;
  uint8_t motionPIR = digitalRead(PIR_PIN);
  if(NIGHTMODE_START > NIGHTMODE_END && (rtctime.hour() >= NIGHTMODE_START || rtctime.hour() < NIGHTMODE_END) && !(motionPIR == HIGH)){
    // nightmode duration over night (e.g. 22:00 -> 6:00)
    nightmode = true;
  }
  else if(NIGHTMODE_START < NIGHTMODE_END && (rtctime.hour() >= NIGHTMODE_START && rtctime.hour() < NIGHTMODE_END) && !(motionPIR == HIGH)){
    // nightmode duration during day (e.g. 18:00 -> 23:00)
    nightmode = true;
  }
  
  if(!nightmode){
    // nightmode is not active -> draw on Matrix, as normal
    
    // if color mode is set to 0, a color wheel will be shown in background
    if(activeColorID == 0){
      // draw colorwheel on matrix
      drawCircleOnMatrix(offset);
      offset = (offset + 1)%256;
    }

    // draw grid reprentation of time to matrix
    drawOnMatrix();
  }
  else{
    Serial.println("Nightmode active -> LED OFF");
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
  for(int r = 0; r < HEIGHT; r++){
    for(int c = 0; c < WIDTH; c ++){
      int angle = ((int)((atan2(r - CENTER_ROW, c - CENTER_COL) * (180/M_PI)) + 180) % 360);
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

int checkDCFSignal(){
  pinMode(DCF_PIN, INPUT);
  // Measure signal quality DCF77 (takes some seconds - please wait)
  Serial.print("checkDCFSignal... ");

  //Do measurement over 10 impules, one impulse takes exactly one Second
  int q = DCF77signalQuality(10);
  Serial.print(q);
  Serial.print(" ");
  if(q < 16){
    Serial.println("(NO SIGNAL)");
  } else if (q < 70 || q > 124){
    Serial.println("(MISERABLE)");
  } else if (q < 85 || q > 110){
    Serial.println("(BAD)");
  } else {
    Serial.println("(GOOD)");
  }
  
  // map quality value to a scale 0-10 (10 - good, 1 - bad)
  return max(1, 10 - abs(q-100)/4);
}

// check signalquality of DCF77 receiver (code from Ralf Bohnen, 2013
int DCF77signalQuality(int pulses) {
  int prevSensorValue=0;
  int loopTime = 10; // loop cycle length in ms
  // Since we could enter in the middle of a pulse, we discard the first one.
  int rounds = -1; 
  uint32_t gagingStart = 0;
  uint32_t lastToggle = 0;
  uint8_t toggleState = 0;
  int waitingPeriod = 0;
  int overallChange = 0;
  int change = 0;

  while (true) {
    // Our loop shall process the input signal (LOW or HIGH) 100 times per
    // second to ensure this, we measure its execution time.
    gagingStart = millis();
    int sensorValue = digitalRead(DCF_PIN);
    // When changing from LOW to HIGH a new pulse starts
    if (sensorValue==1 && prevSensorValue==0) { 
      rounds++;
      if (rounds > 0 && rounds < pulses + 1) {overallChange+= change;}
      if (rounds == pulses) { return overallChange /pulses;}
      change = 0; 
    }
    prevSensorValue = sensorValue;
    change++;

    // let top left led blink during measurement
    if(millis() - lastToggle > 200){
      lastToggle = millis();
      matrix.drawPixel(0,0, toggleState ? colors[6]: matrix.Color(0,0,0));
      matrix.show();
      toggleState = !toggleState;
    }

    // A change between LOW and HIGH should take place exactly every 100 passes, 
    // if it is greater we have no reception.
    // I have determined 300 as a good value, a higher value would strengthen 
    // the statement but then increases the time.
    if (change > 300) {return 0;}
    // Calculate and adjust the execution time
    waitingPeriod = loopTime - (millis() - gagingStart);
    if(waitingPeriod <= loopTime){
      delay(waitingPeriod);
    }
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

    // id update intervall is over update RTC time with DCF time
    if((millis() - updateTimer) > updateIntervall){
      Serial.println("Adjust RTC time");
      rtc.adjust(currentDCFtime);
      updateTimer = millis();
    }
  }
}

// "activates" a pixel in grid
void gridAddPixel(uint8_t x, uint8_t y){
    writeGrid(x, y, 1);
}

// "deactivates" all pixels in grid
void gridFlush(void){
    //Setzt an jeder Position eine 0
    for(uint8_t i=0; i<HEIGHT; i++){
        for(uint8_t j=0; j<WIDTH; j++){
            writeGrid(j, i, 0);
        }
    }
}

// draws the grid to the ledmatrix with the current active color
void drawOnMatrix(){
  for(int z = 0; z < HEIGHT; z++){
    for(int s = 0; s < WIDTH; s++){
      if(readGrid(s, z) != 0){
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

uint8_t readGrid(uint8_t x, uint8_t y){
  uint16_t id = x*WIDTH + y;
  uint8_t byteId = id / 8;
  uint8_t bitId = id % 8;
  return (grid[byteId] >> bitId) & 0x1;
}

void writeGrid(uint8_t x, uint8_t y, uint8_t val){
  uint16_t id = x*WIDTH + y;
  uint8_t byteId = id / 8;
  uint8_t bitId = id % 8;
  
  if(val > 0){
    grid[byteId] = (0x1 << bitId) | grid[byteId];
  } else {
    grid[byteId] = !(0x1 << bitId) & grid[byteId];
  }
}
