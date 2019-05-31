%% Import test 
%the script is broken up into to section only run the data import portion
%once since it take a considerable amount of time
clear;clc;
cal_file = '522_test_cal.csv'; % put the calibrated file name here 
no_cal_file= '522_test_no_cal.csv';
format long g
CalData = csvread(cal_file,1,1);
NoCalData= csvread(no_cal_file,1,1);

%%
close all
start=220 % change this value to set the start time in seconds
stop=250 % change this value to set the stop time in seconds 
l=length(CalData)
fs=50e3; % sample rate 
bin_count=25 % how many bins for the hist 
%calculate the test time based on sample count
time=(l-1)*1/fs
x=0:1/fs:time;
%convert time to seconds
starts=50e3*start+1 ;
stops=50e3*stop+1;

figure('Name','Calibrated Histogram')
histogram(CalData(starts:stops),bin_count)
title('Calibrated -1g Acceleration Test')
xlabel('g') 
ylabel('Samples per Bin') 



figure('Name','Uncalibrated Histogram')
histogram(NoCalData(starts:stops),bin_count)
title('Uncalibrated Idle Acceleration Test')
xlabel('g') 
ylabel('Samples per Bin') 

figure('Name','Calibrated Data')
plot(x(starts:stops),CalData(starts:stops))
title('Calibrated +/-1g Acceleration Test')
ylabel('g') 
xlabel('Time (Seconds)')
figure('Name','Uncalibrated Data')
plot(x(starts:stops),NoCalData(starts:stops))


%%
close all
pd = fitdist(CalData(starts:stops),'Normal')




