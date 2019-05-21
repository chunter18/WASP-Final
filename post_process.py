import csv
import argparse
import os
import sys
import time


def parse_calib_file(calib_filename, data_filename):

    #find the mac addr from the input (data) filename
    #on windows/mac the : in the filename doesnt come through
    #we need to find the delimiter in use
    #possibilites '/', '_', '%3A', ':'

    if data_filename.find('/') != -1:
        delimiter = '/'
    elif data_filename.find('_') != -1:
        delimiter = '_'
    elif data_filename.find(':') != -1:
        delimiter = ':'
    elif data_filename.find('%3A') != -1:
        delimiter = '%3A'

    #print(delimiter)

    data_filename = data_filename.strip('.csv')
    mac = data_filename.split(delimiter)
    #print(mac)
    

    first = True
    with open(calib_filename, 'r') as file:
        for line in file:
            if first:
                first = False
                continue
            else:
                contents = line.split(', ')
                filemac = contents[0].split(':')
                #print(filemac)
                if filemac == mac:
                    floatstr = contents[1]
                    floatstr = floatstr.strip('\n')
                    #print(floatstr)
                    return float(floatstr)

    #if we got here, we didnt get a match
    printf('ERROR: device not found in calibration data')
    printf('Please speicify a data file for a calibrated device')
    sys.exit() 
    

def sample_to_voltage(sample, refV=3.3):
    #if the vref is 3.3, we can use the precalculeted number to save some time
    if(refV == 3.3):
        step_scaled = 50.35400390625
    else:
        scaled = refV*1000000.0 #1mil
        step_scaled = scaled/65536.0
    
    scaled_v = step_scaled*sample
    V = scaled_v / 1000000.0
    return V

def voltage_to_g(v, vref=3.3):
    #4mv per g for 5v ref
    #2.64 for 3.3 ref
    #range is -500 to 500
    #midscale is 0g

    #we are going to see that floating pint error - how to avoid?

    #v to mv - multiply by 1000

    if(vref == 3.3):
        midrange = 1.65
        mv_per_g = 2.64
    else:
        midrange = vref/2
        mv_per_g = 1000*(vref*(.004/5)) #proportion calc

    if v == midrange:
        return 0
    if v > midrange:
        v2 = v-midrange
        mv = v2*1000
        return (mv/mv_per_g)
    if v < midrange:
        v2 = midrange- v
        mv = v2*1000
        return (-mv/mv_per_g)

if __name__ == '__main__':
    
    parser = argparse.ArgumentParser(description='Turns WASP format csv into single col csv')
    parser.add_argument("input", help="input csv file", type=str)
    parser.add_argument("-c", "--calibration",help="input calibration file", type=str)
    parser.add_argument("-o", "--out", help="filename to write output to, defaults to out.csv", type=str)
    parser.add_argument("-g", "--accel", help="write a acceleration column (assumes 3.3 vref)", action="store_true")
    parser.add_argument("-v", "--voltage", help="write a voltage column (assumes 3.3 vref)", action="store_true")
    parser.add_argument("-a", "--analyze", help="run miss rate analysis", action="store_true")
    args = parser.parse_args()

    exists = os.path.isfile(args.input)
    if not exists:
        print('input file doesnt exist')
        sys.exit()

    if args.calibration:
        exists = os.path.isfile(args.calibration)
        if not exists:
            print('calibration file doesnt exist')
            sys.exit()
        else:
            calib_val = parse_calib_file(args.calibration, args.input)
            calib_val = round(calib_val)
            
    if args.out:
        out_filename = args.out
    else:
        out_filename = 'out.csv'
              
    #find num lines in the file
    with open(args.input, 'r') as file: #relatively quick
        row_count = sum(1 for row in file)
        print('row count = {}'.format(row_count))
        print('output column length = {}'.format(row_count*236))



    longest_missed = 0
    total_missed = 0
    last = 0
    highcount = 0
    rate=1
    midscale = (2**16)/2
                
    
    first = True
    startime = time.time()
    with open(args.input, 'r') as infile:
        with open(out_filename, 'w') as outfile:
            for i, row in enumerate(infile): #uses buffered io, reads one line at a time, good for big files
                if first: #csv header
                    first = False
                    if args.calibration:
                        outfile.write('calibrated sample')
                    else:
                        outfile.write('sample')
                    if args.voltage:
                       outfile.write(',voltage')
                    if args.accel:
                        outfile.write(',accel')
                    outfile.write('\n')
                    continue

                #regular rows
                rowlist = row.split(',')
                packet_count = rowlist[0]
                count = int(packet_count)
               
                highcount = count

                #miss rate analysis
                if last+1 != count:
                    diff = count - last
                    last = count
                    total_missed += diff
                    if diff > longest_missed:
                        longest_missed = diff
                else:
                    last = count
                
                #we only want the samples
                samples = rowlist[3].split(' ')
                del samples[-1] #delete the newline

                #fill in missed sample data with zeros
                if count != rate:
                    delta = count - rate
                    #print('delta=%d' % delta)
                    #print('rate=%d' %rate)
                    for x in range(delta):
                    #the packet count is ahead of where we should be, fill in zeros until its write
                        for _ in range(236):
                            outfile.write('0')#sample
                            if (args.voltage or args.accel):
                                voltage = sample_to_voltage(0)
                                if args.voltage:    
                                    outfile.write(',{}'.format(voltage))
                                if args.accel:
                                    accel = voltage_to_g(voltage)
                                    outfile.write(',{}'.format(accel))
                            outfile.write('\n')
                        rate=rate+1
                rate=rate+1

                   

                

                #we should log the miss rates and stuff

                for sample in samples:
                    if args.calibration:
                        #here we have specified we want to take the calib data into account
                        #instead of writing
                        offset = midscale - calib_val
                        sample = sample + offset
                    outfile.write('{}'.format(sample))
                    if (args.voltage or args.accel):
                        voltage = sample_to_voltage(int(sample))
                        if args.voltage:    
                            outfile.write(',{}'.format(voltage))
                        if args.accel:
                            accel = voltage_to_g(voltage)
                            outfile.write(',{}'.format(accel))
                    outfile.write('\n')
        

                #print(packet_count)
                #print(samples)
                #print(len(samples))
                #print(i)
                    
    endtime = time.time()
    if args.analyze:
        #note - currently we cant guaruntee the accuracy of these items as
        #we could have potientially missed some packets on the end.
        #To correct this, we would need to add a TCP send of the total send
        #count from the client when the test is terminated.
        print('recorded time was {} seconds'.format(endtime-startime))
        print('total packets counted from csv: {}'.format(row_count-1))
        print('highest packet count was {}'.format(highcount))
        print('of those {}, {} total were lost, or {}% of the total'.format(highcount, total_missed, (total_missed/highcount)))
        print('the longest stretch of losses was {} packets'.format(longest_missed)) 
        samp_count = highcount*236
        secs = samp_count/50000
        print('at a sample rate of 50 kHz, that test lasted {:3f} seconds or {:3f} minutes'.format(secs, secs/60))

        
    
