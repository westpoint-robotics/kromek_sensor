# Kromek kernel module

The Kromek driver for Linux is provided in two parts. A kernel module (.ko file) and a dynamic library (.so). 
To use the library, you will need to compile and install the kernel module on your Linux system.
You will need to perform these steps each time your kernel changes.

## Installation

An installation script (install.sh) has been provided to compile and install all the appropriate files. 
This will install the driver on most forms of Linux that use udev and systemd (Tested on Ubuntu + Raspbian) .
To install manually, use the install.sh as a guide.

## Pre-requisties

To build the module you will need to install the current kernel headers.
The install script runs this for Ubuntu. 

On Ubuntu this can achieved by installing the appropriate package:
`sudo apt-get install linux-headers-generic`

On Raspberry Pi
`sudo apt-get install raspberrypi-kernel-headers`

With recent versions of Ubuntu, you need to create a kernel module signing certificate.
The install script will attempt this if needed.
