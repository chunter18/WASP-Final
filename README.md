# WASP

This is the final code submission of my senior design project for Santa Clara University School of Engineering. 

Team memebers:
Tyler Hack
Cole Hunter
Daniel Webber

Advisers:
Dr. Wilson
Dr. Dezfouli

## What is WASP?

WASP (Wireless Analog Sensor Platform) is my teams senior deisng project, the goal of which is to create a simple, powerful, accurate, and compact way to measure accleration for sattelite tests. Here are some snippets from our abstract:

WASP’s goal is to augment and eventually replace the bulky, costly, and inefficient data acquisition systems used for vibrational reliability tests on satellites. This will enable precise testing of conditions on a smaller time frame and at a lower cost.

The project objective is to design a low power, small footprint, wireless sensor module designed to be the next generation in space flight operational testing. Currently, Space and Aerospace companies conduct non-destructive vibrational experiments on their vehicles to ensure that they will survive the high G-forces required to leave the atmosphere. These tests are conducted with over 200 wired accelerometers simultaneously connected to multi-thousand dollar data acquisition modules. These systems result in a disarray of wired connections making installing and debugging these sensors an excessively laborious and time-consuming process. We propose a low-cost, battery-powered module, designed for engineers, to augment and eventually replace the current testing and data acquisition system. The goal is to maintain the accuracy and precision of the current standard of testing, but eliminate the pitfalls of a wired system. These experiments typically last for tens of minutes and up to an hour each time, during which the module will be collecting vibrational data at a very high speed and streaming this data continuously to a base station for analysis. The module must be ultra-low-powered as sensor placement in hard to access locations can occur weeks before the actual experiment takes place. The module will need to be small enough so as not to contribute to system load, which can skew measurements. The final system needs to function in parity with the current system.

As of June 2019, we submitted our final design for a board (including 5 assembled and evaluated boards funded by our project sponsor - Analog Devices) as well as all of the code for the main application we wrote.

In this repository you will find our teams final platform code, the board files, instructions for evaluating one of the rev1 WASP modules, and anything else we want to deliver as a part of the project. To see some more (messy) test code, you can have a look at my other WASP repo, where you can see the expiriments we ran throughout the year.  

## Evaluation Instuctions

For these instructions, we will assume you are famiair with the WICED (Wireless Internet Connectivity for Embedded Devices) software. If you arent, please head over to https://www.cypress.com/products/wiced-software and download WICED studio. We are using version 6.2 for all of our WASP builds, but any version ^ or greater should work.

To start, clone this repository and place it into your WICED studio directory. On windows, the default directory is Documents\WICED-Studio-6.2\43xxx_Wi-Fi\apps. 

Next, we need to modify a few of the base WICED files.
1. First we need to modify wwd_wifi. WASP adds a function to dissasociate with the wifi network to make changing the netowork dynamically easy - the file is otherwise unchanged, so replacing the files wont break any other builds. To do this, replace the wwd_wifi.c in WICED-Studio-6.2\43xxx_Wi-Fi\WICED\WWD\internal\ with the file in the cloned wwd folder, and replace the wwd_wifi.h file in WICED-Studio-6.2\43xxx_Wi-Fi\WICED\WWD\include\ folder.
2. Navigate to https://community.cypress.com/docs/DOC-16139 and follow the steps in the attached document to add the quicksilver platform to the OTA2 libraries. NOTE: it is not necessary to edit the Quicksilver platform.c/.h files for our use case. Hopefully in future versions of WICED these changes will be merged into WICED, so if you notice thats the case you can safely ignore this step. 
3. We will also need to modiy the SPI bitbang code to support MOSI idle high for the ADX4008 ADC. Replace 43xxx_Wi-Fi\WICED\platform\MCU\BCM4390x\peripherals\platform_spi_i2c.c with the one from the repo. 
4. For the final edit, we will need to edit the OpenOCD file for the CYW so that we can use the SEGGER J-link as our JTAG programmer. Replace BCM4390x.cfg with the one from this repo in 43xxx_Wi-Fi\tools\OpenOCD. 

To use the SEGGER J-link as your JTAG programmer, you will also need to follow the instructions at https://community.cypress.com/community/wiced-wifi/wiced-wifi-forums/blog/2018/03/19/downloading-and-debugging-cyw43907-using-jlink-segger. NOTE: this link does not include any instructions to install the j-link drivers - please do this as well.

Next, we need to build the OTA2 extract application that WASPs OTA system relies on. To do so, simple place this make target into your makefiles pane `snip.ota2_extract-Quicksilver_EVL` and double click to build it. This app doesnt need to be downloaded, but it is a prerequsite to building the main app.

Our final step before building is to change some of the WASP defualts to better match your test case.
1. Change the battery capacity in headers/WASP_i2c.h to match your battery of choice. Many popular battery sizes are included, so you may just comment out all but the correct one. NOTE: WASP expects a single celled LiPo battery.
2. Change the server IP address to match you own in headers/wasp_network_global.h
3. Change the wifi networking in wifi_config_dct.c to match your own.

You are no finally ready to build the WASP app! To do so, place the following make target into your list `WASP.wasp_main-Quicksilver_EVL JTAG=jlink ota2_image download_apps download run` and run it. This will build and download the app onto the WASP board for evaluation.

To see what the platform is running, connect any UART to USB device to the WASP board UART headers, and open the associated com port. the WASP app prints some various information to the console, which can be useful for debugging.

In addition to this, the WASP server will need to be running in order to see the full functionality of the test app (otherwise the board will stop after failing to connect over TCP). To build the server, cd into the server directory of the repo and run make. The server will then be ready to run 

troubleshooting
1. Check that the OTA extract was built successfully
2. Check that the built server IP matches the actual IP of the server on the local network. 
3. Feel free to contact me for any other issues you may face. 

# Evaluating the OTA functionality

To use the OTA function of the WASP board, you will need to follow a few additional steps.

1. First, you will need to build your new application to be loaded. To do so, simply add ota2_image to your makefile while removing any of the download commands like so: `WASP.wasp_main-Quicksilver_EVL ota2_image`. This will build the the file `OTA2_image_file.bin` in the build/WASP.wasp_main-Quicksilver_EVL/ directory. This file includes all of the pieces necessary for the full app in a single file (bootloader, filesystem, apps code, dct, etc)
2. Place the included mongoose HTTP server and OTA image file together in the same directory on the server and run the mongoose server.
3. Make sure the OTA flag is set by the TCP portion of the server - TODO
4. Watch it update! you may wish to connect to the WASP board via UART to watch the download progress. The log level has been set to it max allowed level so the UART will be printing a lot of data. You should see the download complete, the board restart, extract the new file, and finllay run the newly updated application.

