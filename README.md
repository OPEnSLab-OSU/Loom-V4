<p align="center">
    <img src="https://github.com/OPEnSLab-OSU/Loom/blob/gh-pages/Aux/OPEnSLogo.png" alt="logo" width="100" height="100">
  </a>
</p>

<h3 align="center">Loom Version 4</h3>
<p align="center">
  An Internet of Things Rapid Prototyping System for applications in environmental sensing
  <br>
  <a href="https://openslab-osu.github.io/Loom-V4">Documentation</a>
  ·
  <a href="https://github.com/OPEnSLab-OSU/Loom-V4/wiki">Project Wiki</a>
  ·
  <a href="#">Quick Start</a>
</p>

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
- [Installation](#Install)
    - [Arduino Install](#install-arduino)
    - [Board Profile Install](#install-board-profiles)
    - [If Using Mac](#if-you-use-mac)
- [Using Examples](#run-an-example)
- [Troubleshooting](#troubleshooting)
- [Opening Issues](#issues)
- [Supported Projects](#opens-supported-projects)

<br>

## Introduction

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

## Run an Example

### Select Board

- Make sure in the Tools > Board menu, that "Loomified Feather M0" is selected
  - If that board is not present, make sure you followed all the instructions above in Installation
  - Plug in your Feather board and then make sure in the Tools > Port menu that you select the device with "Adafruit Feather M0" in the name.
  
### Compile

- Start by compiling our Basic Loom examples (File > Examples > Loom > Sensors > **Analog**) to ensure that it compiles. If not, review the previous steps.
  - Once the Basic example is open, click the checkbox icon "Verify" in the top. (This may take several minutes to compile, this is to be expected) 
  - You should get white-color font text readout mentioning a successful compilation and x% memory used.
![Successful compilation message](https://i.ibb.co/kS8jFbj/Arduino.png)
  - *Note: may disregard orange-color warning text regarding nRF*

## Troubleshooting
If you get permissions error accessing the library folder, abnd are using a Mac, see note in the installation section above.
If you are updating from an old version of Loom, you may need to do a "clean install" by removing the Arduino15 folder, and starting the process from the begining to install again.

## Issues
If you are experiencing issues, please click the "Issues" tab on this repo and choose the template that best fits your needs. Please supply as much information as possible. This helps us to better understand a fix issues quickly

## OPEnS Supported Projects
 - WeatherChimes (Fully Tested)
 - FloDar (Appears Stable)
 - Smart Rock (Appears Stable)
 - Evaporometer (Untested)
 - Dendrometer_Hub (Untested)
 - Lily Pad (Untested)

