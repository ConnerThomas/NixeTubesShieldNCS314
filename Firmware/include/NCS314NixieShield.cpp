#include <Arduino.h>
#include <SPI.h>
#include "NCS314NixieShield.h"

#define UpperDotsMask 0x80000000
#define LowerDotsMask 0x40000000

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
  if (((dotPattern&~tmp)>>6)&1==1) LD=true;//digitalWrite(pinLowerDots, HIGH);
      else LD=false; //digitalWrite(pinLowerDots, LOW);
  if (((dotPattern&~tmp)>>7)&1==1) UD=true; //digitalWrite(pinUpperDots, HIGH);
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

void LEDsOFF()
{
  for(int i=0;i<NUMPIXELS;i++)
  {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0)); 
  }
  pixels.show();
}

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

  analogWrite(RedLedPin,75);
  delay(200);
  analogWrite(RedLedPin,0);
  analogWrite(GreenLedPin,75);
  delay(200);
  analogWrite(GreenLedPin,0);
  analogWrite(BlueLedPin,75);
  delay(200); 
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

void checkAlarmTime()
{
  if (value[Alarm01] == 0) return;
  if ((Alarm1SecondBlock == true) && ((millis() - lastTimeAlarmTriggired) > 1000)) Alarm1SecondBlock = false;
  if (Alarm1SecondBlock == true) return;
  if ((hour() == value[AlarmHourIndex]) && (minute() == value[AlarmMinuteIndex]) && (second() == value[AlarmSecondIndex]))
  {
    lastTimeAlarmTriggired = millis();
    Alarm1SecondBlock = true;
    // Serial.println(F("Wake up, Neo!"));
    p = song;
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
  static String DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
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
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
        if (menuPosition == TemperatureIndex) stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), updateTemperatureString(getTemperature(value[DegreesFormatIndex])));
      } else
      {
        if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), getTimeNow());
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
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
    SPI.beginTransaction(SPISettings(2000000, LSBFIRST, SPI_MODE2));
    else SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE2));
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
  
  /**********************************************************
   * Подготавливаем данные по 32бита 3 раза
   * Формат данных [H1][H2}[M1][M2][S1][Y1][Y2]
   *********************************************************/
   
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
  } else 
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
  } else
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

  p = song;
  parseSong(p);
  //p=0; //need to be deleted
  
  LEDsTest();
  String testStringArray[11] = {"000000", "111111", "222222", "333333", "444444", "555555", "666666", "777777", "888888", "999999", ""};
  testStringArray[10] = FirmwareVersion;

  int dlay = 500;
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