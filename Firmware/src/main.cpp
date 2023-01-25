//Format                _X.XXX_
//NIXIE CLOCK SHIELD NCS314 v 2.x by GRA & AFCH (fominalec@gmail.com)
/**1.95 23.01.2023
 * Conner Thomas modifications to build in VSCODE and some TBD updates
 * TODO: Add nighttime sleep, smoothing, alternate slot machine on longer timer
 * - Added: Fading smoothly between digit changes
 * - Removed IR, tone, GPS, and neopixel code
**/

/************************************************************************************************
 * * * * * * * * * * * * * * * * * * * * * * INCLUDES * * * * * * * * * * * * * * * * * * * * * *
*************************************************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <ClickButton.h>
#include <TimeLib.h>
#ifndef GRA_AND_AFCH_TIME_LIB_MOD
  #error The "Time (TimeLib)" library modified by GRA and AFCH must be used!
#endif
// #include <Tone.h>
// #include <string.h>
#include <EEPROM.h>
#include <OneWire.h>

// #include "NCS314NixieShield.h"

/************************************************************************************************
 * * * * * * * * * * * * * * * * * * * * * * DEFINES * * * * * * * * * * * * * * * * * * * * * *
*************************************************************************************************/

#define TUBES 6

#define TIME_TO_TRY 60000 //1 minute

#define timeModePeriod 60000
#define dateModePeriod 5000

#define US_DateFormat 1
#define EU_DateFormat 0

#define CELSIUS 0
#define FAHRENHEIT 1

#define DS1307_ADDRESS 0x68

#define TimeIndex        0
#define DateIndex        1
#define AlarmIndex       2
#define hModeIndex       3
#define TemperatureIndex 4
#define TimeZoneIndex    5
#define TimeHoursIndex   6
#define TimeMintuesIndex 7
#define TimeSecondsIndex 8
#define DateFormatIndex  9
#define DateDayIndex     10
#define DateMonthIndex   11
#define DateYearIndex    12
#define AlarmHourIndex   13
#define AlarmMinuteIndex 14
#define AlarmSecondIndex 15
#define Alarm01          16
#define hModeValueIndex  17
#define DegreesFormatIndex 18
#define HoursOffsetIndex 19

#define FirstParent      TimeIndex
#define LastParent       TimeZoneIndex
#define SettingsCount    (HoursOffsetIndex+1)
#define NoParent         0
#define NoChild          0

#define DOT_MODE_314 0
//#define DOT_MODE_312 1

#define RHV5222PIN 8

/*************************************************************************************************
 * * * * * * * * * * * * * * * * * * * * * * VARIABLES * * * * * * * * * * * * * * * * * * * * * *
**************************************************************************************************/

bool AttMsgWasShowed=false;

const String FirmwareVersion = "019500";
const char HardwareVersion[] PROGMEM = {"NCS314 for HW 2.x HV5122 or HV5222"};

unsigned int SymbolArray[10]={1, 2, 4, 8, 16, 32, 64, 128, 256, 512}; // I think this is to set up 

// This array is used to write specific states to the HV5122 Sshift register
uint32_t ShiftRegisterArray = 0;

//unsigned int SymbolArray[10]={0xFFFE, 0xFFFD, 0xFFFB, 0xFFF7, 0xFFEF, 0xFFDF, 0xFFBF, 0xFF7F, 0xFEFF, 0xFDFF};

////// CHANGED TO ALLOW FADE DIGITS
// const uint16_t fpsLimit = (uint16_t)49998; // (16666 * 3)
const unsigned int fpsLimit=1000;

const byte LEpin=10; // This pin is the latch for both HV5122 HV controllers
bool HV5222;

const int LEDsDelay=5;

int ModeButtonState = 0;
int UpButtonState = 0;
int DownButtonState = 0;

boolean UD, LD; // DOTS control;

byte data[12];
byte addr[8];
int celsius, fahrenheit;

// These set the sleep hours. Sleep = only colons flash
const int TimeToSleep = 24;
const int TimeToWake = 8;

const byte SleepDim = 5;
const byte RedLedPin = 9; //MCU WDM output for red LEDs 9-g
const byte GreenLedPin = 6; //MCU WDM output for green LEDs 6-b
const byte BlueLedPin = 3; //MCU WDM output for blue LEDs 3-r
const byte pinSet = A0;
const byte pinUp = A2;
const byte pinDown = A1;
const byte pinBuzzer = 2;
//const byte pinBuzzer = 0;
const byte pinUpperDots = 12; //HIGH value light a dots
const byte pinLowerDots = 8; //HIGH value light a dots
const byte pinTemp = 7;
bool RTC_present;
//bool DateFormat=EU_DateFormat;

int rotator = 0; //index in array with RGB "rules" (increse by one on each 255 cycles)
int cycle = 0; //cycles counter
int RedLight = 255;
int GreenLight = 0;
int BlueLight = 0;
unsigned long prevTime = 0; // time of lase tube was lit
unsigned long prevTime4FireWorks = 0; //time of last RGB changed

bool TempPresent = false;


String stringToDisplay = "000000"; // Content of this string will be displayed on tubes (must be 6 chars length)
static String prevstringToDisplay = "000000";
int menuPosition = 0;
// 0 - time
// 1 - date
// 2 - alarm
// 3 - 12/24 hours mode
// 4 - Temperature

byte blinkMask = B00000000; //bit mask for blinkin digits (1 - blink, 0 - constant light)
int blankMask = B00000000; //bit mask for digits (1 - off, 0 - on)

byte dotPattern = B00000000; //bit mask for separeting dots (1 - on, 0 - off)
//B10000000 - upper dots
//B01000000 - lower dots

byte zero = 0x00; //workaround for issue #527
int RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year, RTC_day_of_week;



//-------------------------------0--------1--------2-------3--------4--------5--------6--------7--------8--------9----------10-------11---------12---------13-------14-------15---------16---------17--------18----------19
//                     names:  Time,   Date,   Alarm,   12/24, Temperature,TimeZone,hours,   mintues, seconds, DateFormat, day,    month,   year,      hour,   minute,   second alarm01  hour_format Deg.FormIndex HoursOffset
//                               1        1        1       1        1        1        1        1        1        1          1        1          1          1        1        1        1            1         1        1
int parent[SettingsCount] = {NoParent, NoParent, NoParent, NoParent, NoParent, NoParent, 1,    1,       1,       2,         2,       2,         2,         3,       3,       3,       3,       4,           5,        6};
int firstChild[SettingsCount] = {6,       9,       13,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int lastChild[SettingsCount] = { 8,      12,       16,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int value[SettingsCount] = {     0,       0,       0,      0,       0,       0,       0,       0,       0,  US_DateFormat,  0,       0,         0,         0,       0,       0,       0,       12,     FAHRENHEIT,    2};
int maxValue[SettingsCount] = {  0,       0,       0,      0,       0,       0,       23,      59,      59, US_DateFormat,  31,      12,        99,       23,      59,      59,       1,       24,     FAHRENHEIT,    14};
int minValue[SettingsCount] = {  0,       0,       0,      12,      0,       0,       00,      00,      00, EU_DateFormat,  1,       1,         00,       00,      00,      00,       0,       12,      CELSIUS,     -12};
int blinkPattern[SettingsCount] = {
  B00000000, //0
  B00000000, //1
  B00000000, //2
  B00000000, //3
  B00000000, //4
  B00000000, //5
  B00000011, //6
  B00001100, //7
  B00110000, //8
  B00111111, //9
  B00000011, //10
  B00001100, //11
  B00110000, //12
  B00000011, //13
  B00001100, //14
  B00110000, //15
  B11000000, //16
  B00001100, //17
  B00111111, //18
  B00000011, //19
};

bool editMode = false;

long downTime = 0;
long upTime = 0;
const long settingDelay = 150;
bool BlinkUp = false;
bool BlinkDown = false;
unsigned long enteringEditModeTime = 0;
bool RGBLedsOn = true;
byte RGBLEDsEEPROMAddress = 0;
byte HourFormatEEPROMAddress = 1;
byte AlarmTimeEEPROMAddress = 2; //3,4,5
byte AlarmArmedEEPROMAddress = 6;
byte LEDsLockEEPROMAddress = 7;
byte LEDsRedValueEEPROMAddress = 8;
byte LEDsGreenValueEEPROMAddress = 9;
byte LEDsBlueValueEEPROMAddress = 10;
byte DegreesFormatEEPROMAddress = 11;
byte HoursOffsetEEPROMAddress = 12;
byte DateFormatEEPROMAddress = 13;
byte DotsModeEEPROMAddress = 14;

bool DotsMode=DOT_MODE_314;

uint32_t UpperDotsMask=0x80000000;
uint32_t LowerDotsMask=0x40000000;

int fireforks[] = {0, 0, 1, //1
                   -1, 0, 0, //2
                   0, 1, 0, //3
                   0, 0, -1, //4
                   1, 0, 0, //5
                   0, -1, 0
                  }; //array with RGB rules (0 - do nothing, -1 - decrese, +1 - increse

int functionDownButton = 0;
int functionUpButton = 0;
bool LEDsLock = false;

//antipoisoning transaction
bool modeChangedByUser = false;
bool transactionInProgress = false; //antipoisoning transaction
unsigned long modesChangePeriod = timeModePeriod;
//end of antipoisoning transaction

extern const int LEDsDelay;

byte default_dur = 4;
byte default_oct = 6;
int num;



/************************************************************************************************************
* * * * * * * * * * * * * * * * * * * * * * FUNCTION PROTOTYPES * * * * * * * * * * * * * * * * * * * * * * *
*************************************************************************************************************/

//buttons pins declarations
ClickButton setButton(pinSet, LOW, CLICKBTN_PULLUP);
ClickButton upButton(pinUp, LOW, CLICKBTN_PULLUP);
ClickButton downButton(pinDown, LOW, CLICKBTN_PULLUP);

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w = 1);
OneWire ds(pinTemp);
word doEditBlink(int pos);
word blankDigit(int pos);
String PreZero(int digit);
String getTimeNow();
String updateDisplayString();
// void LEDsOFF(); // This isn't used.
void LEDsTest();
void doDotBlink();
byte decToBcd(byte val);
byte bcdToDec(byte val);
void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w);
void getRTCTime();
int extractDigits(byte b);
void injectDigits(byte b, int value);
bool isValidDate();
void incrementValue();
void decrementValue();
void setLEDsFromEEPROM();
String antiPoisoning2(String fromStr, String toStr);
void testDS3231TempSensor();
String updateDateString();
float getTemperature (boolean bTempFormat);
String updateTemperatureString(float fDegrees);
void modesChanger();
void SPISetup();
void LEDsSetup();
void rotateFireWorks();
void doIndication();
void doTest();
void startupTubes();

/******************************************************************************************************
* * * * * * * * * * * * * * * * * * * * Init Programm * * * * * * * * * * * * * * * * * * * * * * * * * 
*******************************************************************************************************/
void setup()
{

  pinMode(LEpin, OUTPUT);
  digitalWrite(LEpin, LOW);

  Wire.begin();
  //setRTCDateTime(23,40,00,25,7,15,1);

  Serial.begin(115200);

  if (EEPROM.read(HourFormatEEPROMAddress) != 12) value[hModeValueIndex] = 24; else value[hModeValueIndex] = 12;
  if (EEPROM.read(RGBLEDsEEPROMAddress) != 0) RGBLedsOn = true; else RGBLedsOn = false;
  if (EEPROM.read(AlarmTimeEEPROMAddress) == 255) value[AlarmHourIndex] = 0; else value[AlarmHourIndex] = EEPROM.read(AlarmTimeEEPROMAddress);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 1) == 255) value[AlarmMinuteIndex] = 0; else value[AlarmMinuteIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 1);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 2) == 255) value[AlarmSecondIndex] = 0; else value[AlarmSecondIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 2);
  if (EEPROM.read(AlarmArmedEEPROMAddress) == 255) value[Alarm01] = 0; else value[Alarm01] = EEPROM.read(AlarmArmedEEPROMAddress);
  if (EEPROM.read(LEDsLockEEPROMAddress) == 255) LEDsLock = false; else LEDsLock = EEPROM.read(LEDsLockEEPROMAddress);
  if (EEPROM.read(DegreesFormatEEPROMAddress) == 255) value[DegreesFormatIndex] = CELSIUS; else value[DegreesFormatIndex] = EEPROM.read(DegreesFormatEEPROMAddress);
  if (EEPROM.read(HoursOffsetEEPROMAddress) == 255) value[HoursOffsetIndex] = value[HoursOffsetIndex]; else value[HoursOffsetIndex] = EEPROM.read(HoursOffsetEEPROMAddress) + minValue[HoursOffsetIndex];
  if (EEPROM.read(DateFormatEEPROMAddress) == 255) value[DateFormatIndex] = value[DateFormatIndex]; else value[DateFormatIndex] = EEPROM.read(DateFormatEEPROMAddress);

 /* Serial.print(F("led lock="));
  Serial.println(LEDsLock);*/

  pinMode(RedLedPin, OUTPUT);
  pinMode(GreenLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);

// turn off all LEDs
  analogWrite(RedLedPin,0);
  analogWrite(GreenLedPin,0);
  analogWrite(BlueLedPin,0);

  // SPI setup
  SPISetup();
  LEDsSetup();
  //buttons pins inits
  pinMode(pinSet,  INPUT_PULLUP);
  pinMode(pinUp,  INPUT_PULLUP);
  pinMode(pinDown,  INPUT_PULLUP);
  ////////////////////////////
  pinMode(pinBuzzer, OUTPUT);

  //buttons objects inits
  setButton.debounceTime   = 20;   // Debounce timer in ms
  setButton.multiclickTime = 30;  // Time limit for multi clicks
  setButton.longClickTime  = 2000; // time until "held-down clicks" register

  upButton.debounceTime   = 20;   // Debounce timer in ms
  upButton.multiclickTime = 30;  // Time limit for multi clicks
  upButton.longClickTime  = 2000; // time until "held-down clicks" register

  downButton.debounceTime   = 20;   // Debounce timer in ms
  downButton.multiclickTime = 30;  // Time limit for multi clicks
  downButton.longClickTime  = 2000; // time until "held-down clicks" register

  if (EEPROM.read(DotsModeEEPROMAddress) != 255) DotsMode=EEPROM.read(DotsModeEEPROMAddress);
  if (digitalRead(pinDown) == LOW) 
  {
    DotsMode=!DotsMode;
    EEPROM.write(DotsModeEEPROMAddress, DotsMode);
    // tone1.play(1000, 100);
  }
  if (DotsMode == DOT_MODE_314)
  {
    UpperDotsMask=0x80000000;
    LowerDotsMask=0x40000000; 
  }
  else
  {
    UpperDotsMask=0x40000000;
    LowerDotsMask=0x80000000;
  }
  
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  //doTest();
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  if (LEDsLock == 1)
  {
    setLEDsFromEEPROM();
  }
  getRTCTime();
  byte prevSeconds = RTC_seconds;
  unsigned long RTC_ReadingStartTime = millis();
  RTC_present = true;
  while (prevSeconds == RTC_seconds)
  {
    getRTCTime();
    //Serial.println(RTC_seconds);
    if ((millis() - RTC_ReadingStartTime) > 3000)
    {
      // Serial.println(F("Warning! RTC DON'T RESPOND!"));
      RTC_present = false;
      break;
    }
  }
  setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);

  analogWrite(RedLedPin,0);
  analogWrite(GreenLedPin,0);
  analogWrite(BlueLedPin,0);

  startupTubes();

}

/***************************************************************************************************************
  MAIN Program
***************************************************************************************************************/
void loop() {

  if (((millis() % 10000) == 0) && (RTC_present)) //synchronize with RTC every 10 seconds
  {
    getRTCTime();
    setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);
    //Serial.println(F("Sync"));
  }

    if ((millis() - prevTime4FireWorks) > LEDsDelay)
  {
    rotateFireWorks(); //change color (by 1 step)
    prevTime4FireWorks = millis();
  }

  if ((menuPosition == TimeIndex) || (modeChangedByUser == false) ) modesChanger();
  doIndication();

  setButton.Update();
  upButton.Update();
  downButton.Update();
  if (editMode == false)
  {
    blinkMask = B00000000;

  } else if ((millis() - enteringEditModeTime) > 60000)
  {
    editMode = false;
    menuPosition = firstChild[menuPosition];
    blinkMask = blinkPattern[menuPosition];
  }
  if ((setButton.clicks > 0) || (ModeButtonState == 1)) //short click
  {
    modeChangedByUser = true;
    enteringEditModeTime = millis();
    /*if (value[DateFormatIndex] == US_DateFormat)
      {
      //if (menuPosition == )
      } else */
    menuPosition = menuPosition + 1;
#if defined (__AVR_ATmega328P__)
    if (menuPosition == TimeZoneIndex) menuPosition++;// skip TimeZone for Arduino Uno
#endif
    if (menuPosition == LastParent + 1) menuPosition = TimeIndex;
    /*Serial.print(F("menuPosition="));
    Serial.println(menuPosition);
    Serial.print(F("value="));
    Serial.println(value[menuPosition]);*/

    blinkMask = blinkPattern[menuPosition];
    if ((parent[menuPosition - 1] != 0) and (lastChild[parent[menuPosition - 1] - 1] == (menuPosition - 1))) //exit from edit mode
    {
      if ((parent[menuPosition - 1] - 1 == 1) && (!isValidDate()))
      {
        menuPosition = DateDayIndex;
        return;
      }
      editMode = false;
      menuPosition = parent[menuPosition - 1] - 1;
      if (menuPosition == TimeIndex) setTime(value[TimeHoursIndex], value[TimeMintuesIndex], value[TimeSecondsIndex], day(), month(), year());
      if (menuPosition == DateIndex)
      {
        /*Serial.print("Day:");
        Serial.println(value[DateDayIndex]);
        Serial.print("Month:");
        Serial.println(value[DateMonthIndex]);*/
        setTime(hour(), minute(), second(), value[DateDayIndex], value[DateMonthIndex], 2000 + value[DateYearIndex]);
        EEPROM.write(DateFormatEEPROMAddress, value[DateFormatIndex]);
      }
      if (menuPosition == AlarmIndex) {
        EEPROM.write(AlarmTimeEEPROMAddress, value[AlarmHourIndex]);
        EEPROM.write(AlarmTimeEEPROMAddress + 1, value[AlarmMinuteIndex]);
        EEPROM.write(AlarmTimeEEPROMAddress + 2, value[AlarmSecondIndex]);
        EEPROM.write(AlarmArmedEEPROMAddress, value[Alarm01]);
      };
      if (menuPosition == hModeIndex) EEPROM.write(HourFormatEEPROMAddress, value[hModeValueIndex]);
      if (menuPosition == TemperatureIndex)
      {
        EEPROM.write(DegreesFormatEEPROMAddress, value[DegreesFormatIndex]);
      }
      if (menuPosition == TimeZoneIndex) EEPROM.write(HoursOffsetEEPROMAddress, value[HoursOffsetIndex] - minValue[HoursOffsetIndex]);
      //if (menuPosition == hModeIndex) EEPROM.write(HourFormatEEPROMAddress, value[hModeValueIndex]);
      setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);
      return;
    } //end exit from edit mode
    /*Serial.print("menu pos=");
    Serial.println(menuPosition);
    Serial.print("DateFormat");
    Serial.println(value[DateFormatIndex]);*/
    if ((menuPosition != HoursOffsetIndex) &&
        (menuPosition != DateFormatIndex) &&
        (menuPosition != DateDayIndex)) value[menuPosition] = extractDigits(blinkMask);
  }
  if ((setButton.clicks < 0) || (ModeButtonState == -1)) //long click
  {
    // tone1.play(1000, 100);
    if (!editMode)
    {
      enteringEditModeTime = millis();
      if (menuPosition == TimeIndex) stringToDisplay = PreZero(hour()) + PreZero(minute()) + PreZero(second()); //temporary enabled 24 hour format while settings
    }
    if (menuPosition == DateIndex)
    {
      //Serial.println("DateEdit");
      value[DateDayIndex] =  day();
      value[DateMonthIndex] = month();
      value[DateYearIndex] = year() % 1000;
      if (value[DateFormatIndex] == EU_DateFormat) stringToDisplay = PreZero(value[DateDayIndex]) + PreZero(value[DateMonthIndex]) + PreZero(value[DateYearIndex]);
      else stringToDisplay = PreZero(value[DateMonthIndex]) + PreZero(value[DateDayIndex]) + PreZero(value[DateYearIndex]);
      //Serial.print("str=");
      //Serial.println(stringToDisplay);
    }
    menuPosition = firstChild[menuPosition];
    if (menuPosition == AlarmHourIndex) {
      value[Alarm01] = 1; /*digitalWrite(pinUpperDots, HIGH);*/dotPattern = B10000000;
    }
    editMode = !editMode;
    blinkMask = blinkPattern[menuPosition];
    if ((menuPosition != DegreesFormatIndex) &&
        (menuPosition != HoursOffsetIndex) &&
        (menuPosition != DateFormatIndex))
      value[menuPosition] = extractDigits(blinkMask);
    /*Serial.print(F("menuPosition="));
    Serial.println(menuPosition);
    Serial.print(F("value="));
    Serial.println(value[menuPosition]);*/
  }

  if (upButton.clicks != 0) functionUpButton = upButton.clicks;

  if ((upButton.clicks > 0) || (UpButtonState == 1))
  {
    modeChangedByUser = true;
    incrementValue();
    if (!editMode)
    {
      LEDsLock = false;
      EEPROM.write(LEDsLockEEPROMAddress, 0);
    }
  }

  if (functionUpButton == -1 && upButton.depressed == true)
  {
    BlinkUp = false;
    if (editMode == true)
    {
      if ( (millis() - upTime) > settingDelay)
      {
        upTime = millis();// + settingDelay;
        incrementValue();
      }
    }
  } else BlinkUp = true;

  if (downButton.clicks != 0) functionDownButton = downButton.clicks;

  if ((downButton.clicks > 0) || (DownButtonState == 1))
  {
    modeChangedByUser = true;
    decrementValue();
    if (!editMode)
    {
      LEDsLock = true;
      EEPROM.write(LEDsLockEEPROMAddress, 1);
      EEPROM.write(LEDsRedValueEEPROMAddress, RedLight);
      EEPROM.write(LEDsGreenValueEEPROMAddress, GreenLight);
      EEPROM.write(LEDsBlueValueEEPROMAddress, BlueLight);
      /*Serial.println(F("Store to EEPROM:"));
      Serial.print(F("RED="));
      Serial.println(RedLight);
      Serial.print(F("GREEN="));
      Serial.println(GreenLight);
      Serial.print(F("Blue="));
      Serial.println(BlueLight);*/
    }
  }

  if (functionDownButton == -1 && downButton.depressed == true)
  {
    BlinkDown = false;
    if (editMode == true)
    {
      if ( (millis() - downTime) > settingDelay)
      {
        downTime = millis();// + settingDelay;
        decrementValue();
      }
    }
  } else BlinkDown = true;

  if (!editMode)
  {
    if ((upButton.clicks < 0) || (UpButtonState == -1))
    {
      // tone1.play(1000, 100);
      RGBLedsOn = true;
      EEPROM.write(RGBLEDsEEPROMAddress, 1);
      //Serial.println(F("RGB=on"));
      setLEDsFromEEPROM();
    }
    if ((downButton.clicks < 0) || (DownButtonState == -1))
    {
      // tone1.play(1000, 100);
      RGBLedsOn = false;
      EEPROM.write(RGBLEDsEEPROMAddress, 0);
      //Serial.println(F("RGB=off"));
    }
  }

  switch (menuPosition)
  {
    case TimeIndex: //time mode
      if (!transactionInProgress) stringToDisplay = updateDisplayString();
      doDotBlink();
      blankMask = B00000000;
      break;
    case DateIndex: //date mode
      if (!transactionInProgress) stringToDisplay = updateDateString();
      dotPattern = B01000000; //turn on lower dots
      blankMask = B00000000;
      break;
    case AlarmIndex: //alarm mode
      stringToDisplay = PreZero(value[AlarmHourIndex]) + PreZero(value[AlarmMinuteIndex]) + PreZero(value[AlarmSecondIndex]);
      blankMask = B00000000;
      if (value[Alarm01] == 1) /*digitalWrite(pinUpperDots, HIGH);*/ dotPattern = B10000000; //turn on upper dots
      else
      {
        /*digitalWrite(pinUpperDots, LOW);
          digitalWrite(pinLowerDots, LOW);*/
        dotPattern = B00000000; //turn off upper dots
      }
      break;
    case hModeIndex: //12/24 hours mode
      stringToDisplay = "00" + String(value[hModeValueIndex]) + "00";
      blankMask = B00110011;
      dotPattern = B00000000; //turn off all dots
      break;
    case TemperatureIndex: //missed break
    case DegreesFormatIndex:
      if (!transactionInProgress)
      {
        stringToDisplay = updateTemperatureString(getTemperature(value[DegreesFormatIndex]));
        if (value[DegreesFormatIndex] == CELSIUS)
        {
          blankMask = B00110001;
          dotPattern = B01000000;
        }
        else
        {
          blankMask = B00100011;
          dotPattern = B00000000;
        }
      }

      if (getTemperature(value[DegreesFormatIndex]) < 0) dotPattern |= B10000000;
      else dotPattern &= B01111111;
      break;
    case TimeZoneIndex:
    case HoursOffsetIndex:
      stringToDisplay = String(PreZero(value[HoursOffsetIndex])) + "0000";
      blankMask = B00001111;
      if (value[HoursOffsetIndex] >= 0) dotPattern = B00000000; //turn off all dots
      else dotPattern = B10000000; //turn on upper dots
      break;
    case DateFormatIndex:
      if (value[DateFormatIndex] == EU_DateFormat)
      {
        stringToDisplay = "311299";
        blinkPattern[DateDayIndex] = B00000011;
        blinkPattern[DateMonthIndex] = B00001100;
      }
      else
      {
        stringToDisplay = "123199";
        blinkPattern[DateDayIndex] = B00001100;
        blinkPattern[DateMonthIndex] = B00000011;
      }
      break;
    case DateDayIndex:
    case DateMonthIndex:
    case DateYearIndex:
      if (value[DateFormatIndex] == EU_DateFormat) stringToDisplay = PreZero(value[DateDayIndex]) + PreZero(value[DateMonthIndex]) + PreZero(value[DateYearIndex]);
      else stringToDisplay = PreZero(value[DateMonthIndex]) + PreZero(value[DateDayIndex]) + PreZero(value[DateYearIndex]);
      break;
  }
}



/*************************************************************************************************************
* * * * * * * * * * * * * * * * * * * * * * FUNCTION DEFINITIONS * * * * * * * * * * * * * * * * * * * * * * *
**************************************************************************************************************/

word doEditBlink(int pos)
{
  /*
  if (!BlinkUp) return 0;
  if (!BlinkDown) return 0;
  */

  if (!BlinkUp) return 0xFFFF;
  if (!BlinkDown) return 0xFFFF;
  //if (pos==5) return 0xFFFF; //need to be deleted for testing purpose only!
  int lowBit=blinkMask>>pos;
  lowBit=lowBit&B00000001;
  
  static unsigned long lastTimeEditBlink=millis();
  static bool blinkState=false;
  word mask=0xFFFF;
  static int tmp=0;//blinkMask;
  if ((millis()-lastTimeEditBlink)>300) 
  {
    lastTimeEditBlink=millis();
    blinkState=!blinkState;
    if (blinkState) tmp= 0;
      else tmp=blinkMask;
  }
  if ( (((dotPattern & ~tmp) >> 6) & 1) == 1) LD=true;//digitalWrite(pinLowerDots, HIGH);
      else LD=false; //digitalWrite(pinLowerDots, LOW);
  if ( (((dotPattern & ~tmp) >> 7) & 1) == 1) UD=true; //digitalWrite(pinUpperDots, HIGH);
      else UD=false; //digitalWrite(pinUpperDots, LOW);
      
  if ((blinkState==true) && (lowBit==1)) mask=0x3C00;//mask=B11111111;
  //Serial.print("doeditblinkMask=");
  //Serial.println(mask, BIN);
  return mask;
}

word blankDigit(int pos)
{
  int lowBit = blankMask >> pos;
  lowBit = lowBit & B00000001;
  word mask = 0;
  if (lowBit == 1) mask = 0xFFFF;
  return mask;
}

String PreZero(int digit)
{
  digit = abs(digit);
  if (digit < 10) return String("0") + String(digit);
  else return String(digit);
}

String getTimeNow()
{
  if (value[hModeValueIndex] == 24) return PreZero(hour()) + PreZero(minute()) + PreZero(second());
  else return PreZero(hourFormat12()) + PreZero(minute()) + PreZero(second());
}

String updateDisplayString()
{
  static int prevS = -1;
  prevstringToDisplay = PreZero(hourFormat12()) + PreZero(minute()) + PreZero(second()-1);
  if (second() != prevS)
  {
    prevS = second();
    return getTimeNow();
  } else return stringToDisplay;
}

// void LEDsOFF()
// {
//   for(int i=0;i<NUMPIXELS;i++)
//   {
//     pixels.setPixelColor(i, pixels.Color(0, 0, 0)); 
//   }
//   pixels.show();
// }

void LEDsTest()
{
  // for(int i=0;i<NUMPIXELS;i++)
  // {
  //   pixels.setPixelColor(i, pixels.Color(255, 0, 0)); 
  // }
  // pixels.show(); // This sends the updated pixel color to the hardware.
  // delay(1000);
  // for(int i=0;i<NUMPIXELS;i++)
  // {
  //   pixels.setPixelColor(i, pixels.Color(0, 255, 0)); 
  // }
  // pixels.show(); // This sends the updated pixel color to the hardware.
  // delay(1000);
  // for(int i=0;i<NUMPIXELS;i++)
  // {
  //   pixels.setPixelColor(i, pixels.Color(0, 0, 255)); 
  // }
  // pixels.show(); // This sends the updated pixel color to the hardware.
  // delay(1000);
  // LEDsOFF();

  analogWrite(RedLedPin,100);
  delay(300);
  analogWrite(RedLedPin,0);
  analogWrite(GreenLedPin,100);
  delay(300);
  analogWrite(GreenLedPin,0);
  analogWrite(BlueLedPin,100);
  delay(300); 
  analogWrite(BlueLedPin,0);

}

void doDotBlink()
{
  //dotPattern = B11000000; return; //always on
  //dotPattern = B00000000; return; //always off
  if (second() % 2 == 0) dotPattern = B11000000;
  // if ((millis() % 1000) > 500) dotPattern = B11000000;
  else dotPattern = B00000000;
}

byte decToBcd(byte val) {
  // Convert normal decimal numbers to binary coded decimal
  return ( (val / 10 * 16) + (val % 10) );
}

byte bcdToDec(byte val)  {
  // Convert binary coded decimal to normal decimal numbers
  return ( (val / 16 * 10) + (val % 16) );
}

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w)
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero); //stop Oscillator

  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  Wire.write(decToBcd(w));
  Wire.write(decToBcd(d));
  Wire.write(decToBcd(mon));
  Wire.write(decToBcd(y));

  Wire.write(zero); //start

  Wire.endTransmission();

}

void getRTCTime()
{
  // I think the chip actually being used is DS3231, with an internal crystal. It has the same address, 0x68
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  RTC_seconds = bcdToDec(Wire.read());
  RTC_minutes = bcdToDec(Wire.read());
  RTC_hours = bcdToDec(Wire.read() & 0b111111); //24 hour time
  RTC_day_of_week = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  RTC_day = bcdToDec(Wire.read());
  RTC_month = bcdToDec(Wire.read());
  RTC_year = bcdToDec(Wire.read());
}

int extractDigits(byte b)
{
  String tmp = "1";

  if (b == B00000011)
  {
    tmp = stringToDisplay.substring(0, 2);
  }
  if (b == B00001100)
  {
    tmp = stringToDisplay.substring(2, 4);
  }
  if (b == B00110000)
  {
    tmp = stringToDisplay.substring(4);
  }
  return tmp.toInt();
}

void injectDigits(byte b, int value)
{
  if (b == B00000011) stringToDisplay = PreZero(value) + stringToDisplay.substring(2);
  if (b == B00001100) stringToDisplay = stringToDisplay.substring(0, 2) + PreZero(value) + stringToDisplay.substring(4);
  if (b == B00110000) stringToDisplay = stringToDisplay.substring(0, 4) + PreZero(value);
}

bool isValidDate()
{
  int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (value[DateYearIndex] % 4 == 0) days[1] = 29;
  if (value[DateDayIndex] > days[value[DateMonthIndex] - 1]) return false;
  else return true;

}

void incrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    if (menuPosition != hModeValueIndex) // 12/24 hour mode menu position
      value[menuPosition] = value[menuPosition] + 1; else value[menuPosition] = value[menuPosition] + 12;
    if (value[menuPosition] > maxValue[menuPosition])  value[menuPosition] = minValue[menuPosition];
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1) /*digitalWrite(pinUpperDots, HIGH);*/dotPattern = B10000000; //turn on upper dots
      /*else digitalWrite(pinUpperDots, LOW); */ dotPattern = B00000000; //turn off all dots
    }
    if (menuPosition != DateFormatIndex) injectDigits(blinkMask, value[menuPosition]);
    /*Serial.print("value=");
    Serial.println(value[menuPosition]);*/
  }
}

void decrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    if (menuPosition != hModeValueIndex) value[menuPosition] = value[menuPosition] - 1; else value[menuPosition] = value[menuPosition] - 12;
    if (value[menuPosition] < minValue[menuPosition]) value[menuPosition] = maxValue[menuPosition];
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1) /*digitalWrite(pinUpperDots, HIGH);*/ dotPattern = B10000000; //turn on upper dots
      else /*digitalWrite(pinUpperDots, LOW);*/ dotPattern = B00000000; //turn off all dots
    }
    if (menuPosition != DateFormatIndex) injectDigits(blinkMask, value[menuPosition]);
    /*Serial.print("value=");
    Serial.println(value[menuPosition]);*/
  }
}

void setLEDsFromEEPROM()
{
  analogWrite(RedLedPin, EEPROM.read(LEDsRedValueEEPROMAddress));
  analogWrite(GreenLedPin, EEPROM.read(LEDsGreenValueEEPROMAddress));
  analogWrite(BlueLedPin, EEPROM.read(LEDsBlueValueEEPROMAddress));
}

String antiPoisoning2(String fromStr, String toStr)
{
  //static bool transactionInProgress=false;
  //byte fromDigits[6];
  static byte toDigits[6];
  static byte currentDigits[6];
  static byte iterationCounter = 0;
  if (!transactionInProgress)
  {
    transactionInProgress = true;
    blankMask = B00000000;
    for (int i = 0; i < 6; i++)
    {
      currentDigits[i] = fromStr.substring(i, i + 1).toInt();
      toDigits[i] = toStr.substring(i, i + 1).toInt();
    }
  }
  for (int i = 0; i < 6; i++)
  {
    if (iterationCounter < 10) currentDigits[i]++;
    else if (currentDigits[i] != toDigits[i]) currentDigits[i]++;
    if (currentDigits[i] == 10) currentDigits[i] = 0;
  }
  iterationCounter++;
  if (iterationCounter == 20)
  {
    iterationCounter = 0;
    transactionInProgress = false;
  }
  String tmpStr;
  for (int i = 0; i < 6; i++)
    tmpStr += currentDigits[i];
  return tmpStr;
}

void testDS3231TempSensor()
{
  int8_t DS3231InternalTemperature = 0;
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x11);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 2);
  DS3231InternalTemperature = Wire.read();
  // Serial.print(F("DS3231_T="));
  // Serial.println(DS3231InternalTemperature);
  if ((DS3231InternalTemperature < 5) || (DS3231InternalTemperature > 60))
  {
    // Serial.println(F("Faulty DS3231!"));
    for (int i = 0; i < 5; i++)
    {
      // tone1.play(1000, 1000);
      delay(2000);
    }
  }
}

String updateDateString()
{
  static unsigned long lastTimeDateUpdate = millis() + 1001;
  static String DateString = PreZero(month()) + PreZero(day()) + PreZero(year() % 1000);
  static byte prevoiusDateFormatWas = value[DateFormatIndex];
  if (((millis() - lastTimeDateUpdate) > 1000) || (prevoiusDateFormatWas != value[DateFormatIndex]))
  {
    lastTimeDateUpdate = millis();
    if (value[DateFormatIndex] == EU_DateFormat) DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
    else DateString = PreZero(month()) + PreZero(day()) + PreZero(year() % 1000);
  }
  return DateString;
}

float getTemperature (boolean bTempFormat)
{
  byte TempRawData[2];
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0x44); //send make convert to all devices
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0xBE); //send request to all devices

  TempRawData[0] = ds.read();
  TempRawData[1] = ds.read();
  int16_t raw = (TempRawData[1] << 8) | TempRawData[0];
  if (raw == -1) raw = 0;
  float celsius = (float)raw / 16.0;
  float fDegrees;
  if (!bTempFormat) fDegrees = celsius * 10;
  else fDegrees = (celsius * 1.8 + 32.0) * 10;
  //Serial.println(fDegrees);
  return fDegrees;
}

String updateTemperatureString(float fDegrees)
{
  static  unsigned long lastTimeTemperatureString = millis() + 1100;
  static String strTemp = "000000";
  if ((millis() - lastTimeTemperatureString) > 1000)
  {
    //Serial.println("F(Updating temp. str.)");
    lastTimeTemperatureString = millis();
    int iDegrees = round(fDegrees);
    if (value[DegreesFormatIndex] == CELSIUS)
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "00" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 100) strTemp = "000" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 10) strTemp = "0000" + String(abs(iDegrees)) + "0";
    } else
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "00" + String(abs(iDegrees) / 10) + "00";
      if (abs(iDegrees) < 100) strTemp = "000" + String(abs(iDegrees) / 10) + "00";
      if (abs(iDegrees) < 10) strTemp = "0000" + String(abs(iDegrees) / 10) + "00";
    }

    return strTemp;
  }
  return strTemp;
}

void modesChanger()
{
  if (editMode == true) return;
  static unsigned long lastTimeModeChanged = millis();
  static unsigned long lastTimeAntiPoisoningIterate = millis();
  static int transnumber = 0;
  if ((millis() - lastTimeModeChanged) > modesChangePeriod)
  {
    lastTimeModeChanged = millis();
    if (transnumber == 0) {
      menuPosition = DateIndex;
      modesChangePeriod = dateModePeriod;
    }
    if (transnumber == 1) {
      menuPosition = TemperatureIndex;
      modesChangePeriod = dateModePeriod;
      if (!TempPresent) transnumber = 2;
    }
    if (transnumber == 2) {
      menuPosition = TimeIndex;
      modesChangePeriod = timeModePeriod;
    }
    transnumber++;
    if (transnumber > 2) transnumber = 0;

    if (modeChangedByUser == true)
    {
      menuPosition = TimeIndex;
    }
    modeChangedByUser = false;
  }
  if ((millis() - lastTimeModeChanged) < 2000)
  {
    if ((millis() - lastTimeAntiPoisoningIterate) > 100)
    {
      lastTimeAntiPoisoningIterate = millis();
      if (TempPresent)
      {
        if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(updateTemperatureString(getTemperature(value[DegreesFormatIndex])), getTimeNow());
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(month()) + PreZero(day()) + PreZero(year() % 1000) );
        if (menuPosition == TemperatureIndex) stringToDisplay = antiPoisoning2(PreZero(month()) + PreZero(day()) + PreZero(year() % 1000), updateTemperatureString(getTemperature(value[DegreesFormatIndex])));
      } else
      {
        if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(PreZero(month()) + PreZero(day()) + PreZero(year() % 1000), getTimeNow());
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(month()) + PreZero(day()) + PreZero(year() % 1000) );
      }
      // Serial.println("StrTDInToModeChng="+stringToDisplay);
    }
  } else
  {
    transactionInProgress = false;
  }
}

void SPISetup()
{
  pinMode(RHV5222PIN, INPUT_PULLUP);
  HV5222=!digitalRead(RHV5222PIN);
  SPI.begin(); //

  if (HV5222)
  {
    SPI.beginTransaction(SPISettings(2000000, LSBFIRST, SPI_MODE2));
  } else
  {
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE2));
  }
}

void LEDsSetup()
{
  //pixels.begin(); // This initializes the NeoPixel library.
  //pixels.setBrightness(50);
}

void rotateFireWorks()
{
  if (!RGBLedsOn)
  {
    analogWrite(RedLedPin,0 );
    analogWrite(GreenLedPin,0);
    analogWrite(BlueLedPin,0); 
    return;
  }
  RedLight=RedLight+fireforks[rotator*3];
  GreenLight=GreenLight+fireforks[rotator*3+1];
  BlueLight=BlueLight+fireforks[rotator*3+2];
  analogWrite(RedLedPin,RedLight );
  analogWrite(GreenLedPin,GreenLight);
  analogWrite(BlueLedPin,BlueLight);  
  cycle=cycle+1;
  if (cycle==255)
  {  
    rotator=rotator+1;
    cycle=0;
  }
  if (rotator>5) rotator=0;
}

void doIndication()
{
  static unsigned long lastTimeInterval1Started;
  static unsigned long lastTimeInterval2Started;
//  int light = digitalRead(pinLightSens);
  static int fadecycles = 20;

  static int counter2 = 0;
  static int counter3 = 0;
  static int counter4 = fadecycles;
  static int fadebegin = 1;
  long digits=stringToDisplay.toInt();
  static int LastSecond = second();
  if ((micros()-lastTimeInterval1Started)<fpsLimit) return;

if (LastSecond == second()) {
  counter2 ++;
  counter3 ++;
} else {
  counter2 = 0;
  counter3 = 0;
  counter4 = fadecycles;
  fadebegin = 1;
  LastSecond = second();
  lastTimeInterval2Started=micros();
}
  //if (menuPosition==TimeIndex) doDotBlink();
  lastTimeInterval1Started=micros();
  unsigned long Var32=0;
  if (
      (counter2 < 300) 
    & (menuPosition == TimeIndex) 
    & (second() > 0) 
    & ((micros()-lastTimeInterval2Started) > 100) 
//    & (!light)
    // & (hour() < TimeToSleep) 
    // & (hour() >= TimeToWake)
    & !transactionInProgress
      )
    {
      if (fadebegin == 1) {
        if (((counter3) <= counter4)) 
        {
      digits = prevstringToDisplay.toInt();
    } else {
      counter4 --;
      counter3 = 0;
      digits=stringToDisplay.toInt();   
      }
      if (counter4 <= 2) {
        fadebegin = 0;
        counter4 = 0;
        counter3 = 0;
        }
    } else {
    if (((counter3) <= counter4)) {
      digits = stringToDisplay.toInt();
      } else {
        counter4 ++;
        counter3 = 0;
        digits = prevstringToDisplay.toInt(); 
        }
      }
    } else digits=stringToDisplay.toInt();
 
  //long digits=stringToDisplay.toInt();
  //long digits=12345678;
  //Serial.print("strtoD=");
  //Serial.println(stringToDisplay);
   
  digitalWrite(LEpin, LOW); 

 //-------- REG 1 ----------------------------------------------- 
  Var32=0;
 
  Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(5))<<20; // s2
  digits=digits/10;

  Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(4))<<10; //s1
  digits=digits/10;

  Var32|=(unsigned long) (SymbolArray[digits%10]&doEditBlink(3)); //m2
  digits=digits/10;

  if (LD) Var32&=~LowerDotsMask;
    else  Var32|=LowerDotsMask;
  
  if (UD) Var32&=~UpperDotsMask; 
    else  Var32|=UpperDotsMask; 

  if (HV5222) 
  {
    SPI.transfer(Var32);
    SPI.transfer(Var32>>8);
    SPI.transfer(Var32>>16);
    SPI.transfer(Var32>>24);
  }
  else 
  {
    SPI.transfer(Var32>>24);
    SPI.transfer(Var32>>16);
    SPI.transfer(Var32>>8);
    SPI.transfer(Var32);
  }

 //-------------------------------------------------------------------------

 //-------- REG 0 ----------------------------------------------- 
  Var32=0;
  
  Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(2))<<20; // m1
  digits=digits/10;

  Var32|= (unsigned long)(SymbolArray[digits%10]&doEditBlink(1))<<10; //h2
  digits=digits/10;

  Var32|= (unsigned long)SymbolArray[digits%10]&doEditBlink(0); //h1
  digits=digits/10;

  if (LD) Var32&=~LowerDotsMask;  
    else  Var32|=LowerDotsMask;
  
  if (UD) Var32&=~UpperDotsMask; 
    else  Var32|=UpperDotsMask; 
     
  if (HV5222) 
  {
    SPI.transfer(Var32);
    SPI.transfer(Var32>>8);
    SPI.transfer(Var32>>16);
    SPI.transfer(Var32>>24);
  } 
  else
  {
    SPI.transfer(Var32>>24);
    SPI.transfer(Var32>>16);
    SPI.transfer(Var32>>8);
    SPI.transfer(Var32);
  }
//   if ((hour() < TimeToSleep) & (hour() >= TimeToWake))
// //  if (!light)
//   {
//     digitalWrite(LEpin, HIGH);
//   } else {    
//     if ((counter2 % SleepDim) == 0) digitalWrite(LEpin, HIGH);   
//   }
  digitalWrite(LEpin, HIGH);
//-------------------------------------------------------------------------
}

void doTest()
{
  // Serial.print(F("Firmware version: "));
  // Serial.println(FirmwareVersion.substring(1, 2) + "." + FirmwareVersion.substring(2, 5));
  // Serial.println(PSTR(HardwareVersion));
  // for (byte k = 0; k < strlen_P(HardwareVersion); k++) {
  //   Serial.print((char)pgm_read_byte_near(HardwareVersion + k));
  // }
  // Serial.println();
  // Serial.println(F("Start Test"));

  
  LEDsTest();
  String testStringArray[11] = {"000000", "111111", "222222", "333333", "444444", "555555", "666666", "777777", "888888", "999999", ""};
  testStringArray[10] = FirmwareVersion;

  unsigned int dlay = 500;
  bool test = 1;
  byte strIndex = -1;
  unsigned long startOfTest = millis() + 1000; //disable delaying in first iteration
  bool digitsLock = false;
  while (test)
  {
    if (digitalRead(pinDown) == 0) digitsLock = true;
    if (digitalRead(pinUp) == 0) digitsLock = false;

    if ((millis() - startOfTest) > dlay)
    {
      startOfTest = millis();
      if (!digitsLock) strIndex = strIndex + 1;
      if (strIndex == 10) dlay = 2000;
      if (strIndex > 10) {
        test = false;
        strIndex = 10;
      }

      stringToDisplay = testStringArray[strIndex];
      // Serial.println(stringToDisplay);
      doIndication();
    }
    delayMicroseconds(2000);
  };

  if ( !ds.search(addr))
  {
    // Serial.println(F("Temp. sensor not found."));
  } else TempPresent = true;

  testDS3231TempSensor();

  // Serial.println(F("Stop Test"));
  // while(1);
}

/**
 * @brief Function to test some routines with the high voltage shift register. 
*/
void startupTubes()
{
  int delay = 50; // this must never be less than 1ms, so delay is fine to use

  digitalWrite(LEpin, LOW);

  uint32_t nixie_tube_data = 0;
  SPI.transfer(nixie_tube_data>>24);
  SPI.transfer(nixie_tube_data>>16);
  SPI.transfer(nixie_tube_data>>8);
  SPI.transfer(nixie_tube_data);

  nixie_tube_data = 0;
  SPI.transfer(nixie_tube_data>>24);
  SPI.transfer(nixie_tube_data>>16);
  SPI.transfer(nixie_tube_data>>8);
  SPI.transfer(nixie_tube_data);

  digitalWrite(LEpin, LOW);

  _delay_ms(delay);

  nixie_tube_data = 0x0000000F;

  for (int i = 0; i < 30; i++)
  {
    SPI.transfer(0);
    SPI.transfer(0);
    SPI.transfer(0);
    SPI.transfer(0);

    SPI.transfer(nixie_tube_data>>24);
    SPI.transfer(nixie_tube_data>>16);
    SPI.transfer(nixie_tube_data>>8);
    SPI.transfer(nixie_tube_data);

    nixie_tube_data = nixie_tube_data<<1;

    digitalWrite(LEpin, HIGH);
    _delay_ms(delay);
    digitalWrite(LEpin, LOW);
  }

  nixie_tube_data = 0x0000000F;
  
  for (int i = 0; i < 30; i++)
  {
    SPI.transfer(nixie_tube_data>>24);
    SPI.transfer(nixie_tube_data>>16);
    SPI.transfer(nixie_tube_data>>8);
    SPI.transfer(nixie_tube_data);

    SPI.transfer(0);
    SPI.transfer(0);
    SPI.transfer(0);
    SPI.transfer(0);

    nixie_tube_data = nixie_tube_data<<1;

    digitalWrite(LEpin, HIGH);
    _delay_ms(delay);
    digitalWrite(LEpin, LOW);
  }

}