#!/usr/bin/python3

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import argparse
import random
import serial
import multiprocessing as mp
#from timeit import default_timer as timer

filename = 'log.csv'

def serialtest(uart, sharedval):
	
        arduino = serial.Serial(uart, 9600)
        while 1:
                a = arduino.read()
                a = str(a, 'ascii')
                if a == '#':
                        break
        
        packet = []
        while 1:
                a = arduino.read()
                a = str(a, 'ascii')
                if a == '|':
                        #first 2 are always /r/n
                        packet = packet[2::]
                        #get string
                        packetstr = ''.join(packet)
                        #split into vals
                        l = packetstr.split(',')
                        print(l)
                        
                        #busv.append(l[0])
                        #shuntv.append(l[1])
                        #loadv.append(l[2])
                        #currentmA.append(l[3])
                        
                        sharedval.value = float(l[3])
                        with open(filename, "a") as file:
                                file.write(packetstr)
                                file.write('\n')
                        packet = []
                else:
                        packet.append(a)                                       
                                     
		
def animate(i, xs, ys, ax, powerval, numsamples):
        xs.append(i)
        ys.append(powerval.value)

        xs = xs[-numsamples:]
        ys = ys[-numsamples:]

        ax.clear()
        ax.plot(xs, ys)
        plt.xticks(rotation=45, ha='right')
        plt.subplots_adjust(bottom=0.30)
        plt.title('current ma')
        plt.ylabel('Sample')


def plot(powerval, numsamples):
        print('proc started')
        fig = plt.figure()
        ax = fig.add_subplot(1, 1, 1)

        xs = []
        ys = []

        ani = animation.FuncAnimation(fig, animate, fargs=(xs, ys, ax, powerval, numsamples), interval=500)
        plt.show()

if __name__ == '__main__':
      
        
        csvstr = 'bus_voltage,shunt_voltage,load_voltage,current_mA,power_mW'
        #start = timer()
        with open(filename, "w") as file:
                file.write(csvstr)
                file.write('\n')
        #end = timer()
        #print(end-start) #took 0.004 sec


        parser = argparse.ArgumentParser(description='Graphing utility for INA power measurements on WASP board rev 1')
        #pcb_test_graph.py /ttlfiledesc -d x


        parser.add_argument("UART", help="device descriptor for UART", type=str)
        #parser.add_argument("-d", "--delay", help="delay between text value prints", type=int)
        parser.add_argument("-s", "--samples", help="number of samples to plot at a time", type=int)
        #parser.add_argument("-r", "--random", help="use random samples for testing", type=int)


        args = parser.parse_args()

        #delay = 0
        numsamples = 20
        #random = False

        #if args.delay:
        #	delay = args.delay
        if args.samples:
        	numsamples = args.samples
        #if args.random:
        #	random = True

        shared = mp.Value('d',0)
        plotter = mp.Process(target=plot, args=(shared,numsamples))

        plotter.start()

        serialtest(args.UART, shared)

