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
- [Feather Idiosyncrasies](#weird-feather-m0-issues)
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

## Getting Started Resources
1. Loom Manager Walkthrough: https://media.oregonstate.edu/media/t/1_csq6zrae
2. Loom Hypnos Walkthrough: https://media.oregonstate.edu/media/t/1_7owroc4a
3. Hypnos Interrupts: https://media.oregonstate.edu/media/t/1_7hq18o2d
4. C Tutorials https://www.cprogramming.com/tutorial/c-tutorial.html?inl=pf
5. Loom Sensor Hookup Tutorials: https://www.youtube.com/watch?v=AU5vwO4RJqE&list=PLGLI7V_o5-ahvLrscgoZRVfLx5KtgjEVV&index=3

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

## Weird Feather M0 Issues
There are some weird idiosyncrasies with the Feather M0
 - Something that is only found in the datasheet is that pin A7 and pin D9 are linked together and since A7 is the voltage divider this pin sits at around 2v at all times
 - Some interrupt vectors are linked together so multiple pins may use the same interrupt and may then break interrupts below is a list of pins and their corresponding interrupt

    | Pin # | Interrupt # |
    |--- | --- |
    | D11 | 0 |
    | D13 | 1 |
    | D10, A0, A5 | 2 |
    | D12 | 3 |
    | D6, A3 | 4 |
    | A4 | 5 |
    | SDA | 6 |
    | D9, SCL | 7 |
    | A1 | 8 |
    | A2 | 9 |
    | D23, D1 | 10 |
    | D24, D0 | 11 |
    | D5 | 15 |
 - LoRa Must Be Configured Properly based off the selected modem configuration, see issue: https://github.com/OPEnSLab-OSU/Loom-V4/issues/54

## OPEnS Supported Projects
 - WeatherChimes (Fully Tested)
 - FloDar (Appears Stable)
 - Smart Rock (Appears Stable)
 - Evaporometer (Fully Tested)
 - Dendrometer_Hub (Untested)
 - Lily Pad (Untested)




