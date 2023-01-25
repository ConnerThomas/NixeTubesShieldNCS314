# Nixe Tubes Shield NCS314-6
Heavily modified version of GRA-AFCH's original Nixie Clock Shield code.  <br>

# Main changes: 
	Added smooth fading to digit transitions based on work done by vasques666 on the gra-afch forums (thank you!)

	Removed IR, GPS, and Tone code. Removed Alarm function. May add this back later. 
	
	Reorganized code to be more c++ compliant. Added function prototypes and renamed some things. 

# TODO:
	Separate out common functions into a libaray to keep main.cpp cleaner

	Add a sleep function to dim the display or not use the digits between configurable hours 

1. This repository content sources of project for Nixie Clock developed by GRA & AFCH.

	Folders description:
  
	FIRMWARE - C++ source code platform.io project. This is built with GCC in VSCODE.
  
	LIBRARIES - Copy of modified Time arduino library that this code depends on. The Actual compiled library is within the Firmware directory.
  
	SCHEMATIC - Schematic for hardware version 2.2 (should be the same as v2.3?)
  
	USB DRIVERS - drivers for USB-to-SERIAL(UART) converters


3. Compatibility:

	Nixie Clock Shield for Arduino MEGA2560 - <b>NCS314-6</b> (Hardware Versions: HW2.3)
	
