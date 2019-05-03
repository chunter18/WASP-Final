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



assume familar with wiced - using v 6.2?
change wwd_wifi to included versions
build ota2 extrct
change battery size, server addr
build wasp

ota instrs
see mongo
