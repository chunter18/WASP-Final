import pandas as pd

#find miss count, miss rate.
#wont be perfectly accurate cause we could miss some packets at very end
#maybe send the final count of packets sent as the final packet after test end?

df = pd.read_csv('test.csv')
#print(df.columns)
#print(df.head())
countslist = list(df['count'])
total_packet_count = len(countslist)

longest_missed = 0
total_missed = 0

last = 0
for count in countslist:
    if last+1 == count: #didnt miss any. update last count and move on to next elt.
        last = count
        continue
    else: #missed at least 1 packet
        diff = count - last
        last = count
        total_missed += diff
        if diff > longest_missed:
            longest_missed = diff


print('total packets counted from csv: {}'.format(total_packet_count))
print('highest packet count was {}'.format(countslist[-1]))
print('of those {}, {} total were lost, or {}% of the total'.format(countslist[-1], total_missed, (total_missed/countslist[-1])))
print('the longest stretch of losses was {} packets'.format(longest_missed))


#non pertinent info, just wanted to see
samp_count = countslist[-1]*236
print('with {} packets, and 236 samples per packet, thats {} samples'.format(countslist[-1], samp_count))
secs = samp_count/100000
print('at a sample rate of 100 kHz, that test lasted {} seconds or {} minutes'.format(secs, secs/60))

'''
total packets counted from csv: 110813
highest packet count was 111942
of those 111942, 1212 total were lost, or 0.010827035429061478% of the total
the longest stretch of losses was 732 packets
with 111942 packets, and 236 samples per packet, thats 26418312 samples
at a sample rate of 100 kHz, that test lasted 264.18312 seconds or 4.403052 minutes
'''
