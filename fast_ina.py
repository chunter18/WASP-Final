#!/usr/bin/python3

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import argparse
import random
import serial
import multiprocessing as mp
#from timeit import default_timer as timer

filename = 'log.csv'


shuntv = []
currentmA = []

def fastserial(uart):
	
        arduino = serial.Serial(uart, 115200) #2kb per second
        while 1:
                a = arduino.read()
                a = str(a, 'ascii')
                if a == '#':
                        break
        
        packet = []
        while 1:
                try:
                        a = arduino.read()
                        a = str(a, 'ascii')
                        if a == '|':
                                #first 2 are always /r/n
                                packet = packet[2::]
                                #get string
                                packetstr = ''.join(packet)
                                #split into vals
                                l = packetstr.split(',')
                                #print(l)
                                
                                
                                shuntv.append(l[0])
                                currentmA.append(l[1])
                                
                                packet = []
                        else:
                                packet.append(a)
                                
                except KeyboardInterrupt:
                        #write contents to the csv
                        print('writing')
                        with open(filename, "a") as file:
                                for shunt, current in zip(shuntv, currentmA)
                                        file.write(shunt)
                                        file.write(',')
                                        file.write(current)
                                        file.write('\n')
                        print('done')
                                     
	
        

if __name__ == '__main__':
      
        
        csvstr = 'shunt_voltage,current_mA'
        #start = timer()
        with open(filename, "w") as file:
                file.write(csvstr)
                file.write('\n')
        #end = timer()
        #print(end-start) #took 0.004 sec - will need to catch a keyboard interrupt


        parser = argparse.ArgumentParser(description='')
        #pcb_test_graph.py /ttlfiledesc -d x


        parser.add_argument("UART", help="device descriptor for UART", type=str)

        args = parser.parse_args()

        #delay = 0
        #numsamples = 20
        #random = False

        #if args.delay:
        #	delay = args.delay
        #if args.samples:
        #	numsamples = args.samples
        #if args.random:
        #	random = True

        #shared = mp.Value('d',0)
        #plotter = mp.Process(target=plot, args=(shared,numsamples))

        #plotter.start()

        fastserial(args.UART)

