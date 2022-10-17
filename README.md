# Loom - Version 4

This version takes a step back and removes a lot of the aspects that make Loom 3 very hard to work with. 
This results in a more open and easy to work with framework that allows user to choose to use Loom and its features,
or simply use the reliable software written for these sensors without the loom overarch.

## Install

The install process is fairly simple:

## Install Arduino

- Download and install the latest version of the [Arduino IDE](https://www.arduino.cc/en/Main/Software)

- NOTE: If installing on Windows, download the Windows Installer
 
- NOTE: If installing on Linux, see the official [Linux install guide](https://www.arduino.cc/en/Guide/Linux) 

## Install Board Profiles
1. Open Arduino IDE 
2. Once Open Click File > Preferences or Ctrl + ,
3. In the text box labeled "Additional Boards Manager URLs" paste this text into the box `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json,https://raw.githubusercontent.com/OPEnSLab-OSU/Loom-V4/main/auxilary/package_loom4_index.json`
4. Press Ok
5. Next click Tools > Board > Boards Manager
6. Search and install the following Board Profiles
   - Arduino SAMD Boards
   - Adafruit SAMD Boards
   - Loom SAMD Boards V4
7. Install the latest version of all three boards as they appear

## If you use Mac:
Give Arduino IDE Full Disk Access:
System Preferences->Security & Privacy->"Privacy" tab->Full Disk Access->+ plus button to add Arduino
You will also need to install Developet Tools, which requires Admin access.

## Project Examples
 - WeatherChimes (Fully Tested)
 - FloDar (Appears Stable)
 - Smart Rock (Appears Stable)
 - Evaporometer (Untested)
 - Dendrometer_Hub (Untested)
 - Lily Pad (Untested)

## Resources
 - [Doxygen](https://openslab-osu.github.io/Loom-V4/)

