#ifndef __NCS314NIXIESHIELD_H
#define __NCS314NIXIESHIELD_H

//#include <stdint.h>
#include <Arduino.h>
unsigned int SymbolArray[10]={1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
//unsigned int SymbolArray[10]={0xFFFE, 0xFFFD, 0xFFFB, 0xFFF7, 0xFFEF, 0xFFDF, 0xFFBF, 0xFF7F, 0xFEFF, 0xFDFF};
////// CHANGE FOR FADE DIGITS
// const uint16_t fpsLimit = (uint16_t)49998; // (16666 * 3)
const unsigned int fpsLimit=1000;

const byte LEpin=10;
#define RHV5222PIN 8
bool HV5222;

#endif