/*
Cole Hunter
Unified Server for WASP project
senior design - 2019
Team: Tyler Hack and Daniel Webber
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h> //is this the right one
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h> //need to add -lpthread to make command
#include <dirent.h>
#include <argp.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "wasp_server.h" //defines and stuff


//gloabal variables - look up tables
char macs[300][6];	//this sets the max num devices to 300
char ips[300][INET_ADDRSTRLEN];
FILE *outputfiles[300]; //one file per wasp board for data storage
uint16_t ports[300];
int packet_counts[300];

//used by the table print function.
time_t rawtimes[300];
int battery_levels[300];
int self_test_statuses[300];
int n_connected = 0;
int n_ready = 0;
int threads_running = 0;
int thread_devices_found = 0;
int device_err_flag = 0;
char *cailbration_file_name;
enum testmode{hibernate, initializing, testing}; //for printing the mode
enum testmode mode;

char *datadir;

typedef struct
{
	uint16_t port;
	int selected;
	int selected_num;
} thread_args_t;

//comand line interface vars
const char *argp_program_version = "WASP server 1.0";
const char *argp_program_bug_address = "<chunter@scu.edu>";

//defining the possible arguments
struct arguments
{
//  char *args[2];          // no required args 
	int verbose;              // verbose flag - maybe adds/removes extra debug stuff from print thread 
	int calibration;          // calibration flag - marks that you want to run calibration step on each board
	char *datadir;            // Argument for -d
	char *niface;			  // network interface to look at ip - will be used for implementing ssdp or the like	
};

//these are the definitions of the args defined above
//order is {NAME, KEY, ARG, FLAGS, DOC}
static struct argp_option options[] =
{
	{"verbose", 	'v', 0, 0, "Add extra debug information to print"},
	{"calibrate", 	'c', 0, 0, "Run calibration step on first connect of each board"},
	{"Data Dir",  	'd', "DATADIR", 0, "Custom directory for all data to be written to"},
	{"Net iface",   'n', "NIFACE", 0, "Network inteface being used"},
	{0} //not sure why null is needed, keeping for saftey
};

//description of non optional args - we have non, so those are blank
static char args_doc[] = "";

//program doc string
static char doc[] =
"WASP server -- Host code to interact with WASP modules for acceleration tests.\vCreated by Cole Hunter, Tyler Hack, and Daniel Webber";

//actual argp structure
static struct argp argp = {options, parse_opt, args_doc, doc};

typedef struct
{
	uint8_t mac[6];
	int sock_fd;
}calib_thread_args_t;
	

//one interesting thing to add would be server discovery insteaed of using a fixed server ip in the board code
int main(int argc, char *argv[])
{
		//main function is just the tcp server until the test starts.
		//only then does it spawn the udp threads, and the main function
		//becomes an async tcp "client", connecting to individual boards
		//to send them commands via tcp.
		
		//comand line interface stuff.
		struct arguments arguments;
		// setting argument defaults
		arguments.datadir = "./";
		arguments.niface = "eth0";
		arguments.calibration = 0; //setting default as false - makes testing random shit quicker for us
								   //the defualt should be true for the final system used by the engineers
		arguments.verbose = 0;
		
		//actual argument parsing
		argp_parse (&argp, argc, argv, 0, 0, &arguments);
		
		int arglen = strlen(arguments.datadir);
		
		if(arglen > 100)
		{
			printf("Error: data directory path too long\n");
			exit(-1);
		}
		if(arguments.datadir[arglen-1] == '/')
		{
			datadir = arguments.datadir;
			//trailing slash present, dont need to sanitize the input
		}
		else
		{
			//didnt get a trailing slash so one needs to be added
			datadir = (char*) malloc((arglen+1)*sizeof(char));
			strncpy(datadir, arguments.datadir, arglen);
			datadir[(arglen)] = '/';
			datadir[(arglen+1)] = '\0';
		}
		
		//make the directory
		DIR* dir = opendir(datadir);
		if(dir)
		{
			printf("directory %s already exists, server will use it\n", datadir);
		}
		else if(ENOENT == errno)
		{
			//printf("Directory %s not found, creating it\n", datadir);
			int status = mkdir(datadir, 0777); //read write and exe permission to everybody
			if(status != 0)
			{
				perror("mkdir");
				exit(-1);
			}
		}
		else
		{
			perror("opendir");
			exit(-1);
		}
		
		
		for(int i = 0; i < 300; i++)
			ports[i] = 0;
		printf("listening for connections\n\n");
		
		//create the modefile before the print thread starts
		FILE *modefile;
		modefile = fopen("mode", "wb");
		fprintf(modefile, "%d", mode);
		fclose(modefile);
		//NOTE: we can create files that are pure macs like 00:a0:50:c4:26:a1.csv on linux
		
		
		//create a file to select the board to send data for graphing
		//we could put this in the mode file, but we have to add a loop to count the lines or change the read structure
		//FILE *graphfile;
		//graphfile = fopen("graph", "wb");
		//fprintf(graphfile, "%d", 0);
		//fclose(graphfile);
		
		if(arguments.calibration == 1)
		{
			//create a calibration file for storing the calibration data returned by the boards
			
			FILE *calibfile;
			//this file is used by the post processing step, so it makes sense to place it in the data dir
			char *fullpath = (char*) malloc((strlen(datadir)+16)*sizeof(char));
			sprintf(fullpath, "%s%s", datadir, "calibration.data");
			cailbration_file_name = fullpath;
			calibfile = fopen(cailbration_file_name, "wb");
			//writing a little header
			fprintf(calibfile, "%s", "device, calibration value\n");
			fclose(calibfile);
		}
		calib_thread_args_t t_args;
		
		//initialize the table print thread
		pthread_t print_thread;
		pthread_create(&print_thread, NULL, print_data, NULL);
		
		
		//tcp server variables
		//main tcp thread stuff
		int n;	//num stuff read by read()
		int listenfd = 0, connfd = 0; //listening socket and connected socket
		struct sockaddr_in serv_addr; 
		struct sockaddr_in client_addr;
		socklen_t client_len;
		char addr_buf[INET_ADDRSTRLEN];
		char buff[sizeof(init_packet_t)]; //buffer for mac recieve
		char sendbuffer[sizeof(init_response_t)];
		init_packet_t init;
		init_response_t response;
		
		//for select
		struct timeval tv; //delay between checking
		fd_set rset;
		
		//set up - fill buffer and server address mem with all zeros
		memset (&serv_addr, '0', sizeof (serv_addr));
		memset (buff, '0', sizeof (buff));
		//set ip/mac data to be null so any val is null terminated
		for(int i = 0; i < 300; i++)
		{
			memset(ips[i], '\0', sizeof(ips[0]));
			memset(macs[i], '\0', sizeof(macs[0]));
		}
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
		serv_addr.sin_port = htons (WASP_TCP_SERVER_PORT); //wasp listens on tcp port 50005
		
		//create socket, bind, and listen
		listenfd = socket (AF_INET, SOCK_STREAM, 0);	//listening socket - tcp (SOCK_STREAM)
		bind (listenfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)); 
		listen (listenfd, 10);	//actually listen on addres
		client_len = sizeof(client_addr); 
		
		FD_ZERO(&rset);
		int ready;
		int nfds = listenfd + 1; //highest num + 1 from man page
		int timer_started = 0;
		time_t timer_begin_time;
		time_t timer_now;
		
		//get the host ip - evtually we will use for an sdp protocol
		//the iface is set via arg for now with the default being eth0
		struct ifreq ifr;
		ifr.ifr_addr.sa_family = AF_INET;
		strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
		if( (ioctl(listenfd, SIOCGIFADDR, &ifr)) < 0)
		{
			perror("ioctl");
			exit(-1);
		}
		//printf("server addr is %s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
		
		while(1)
		{
			FD_SET(listenfd, &rset); //select needs these to be re set each time
			tv.tv_sec = 0;
			tv.tv_usec = 1000; //check every 1 millisec
			ready = select(nfds, &rset, NULL, NULL, &tv);//middle 3 args are read, write, and except fds
			
			if(ready)
			{
				connfd = accept (listenfd, (struct sockaddr*)&client_addr, &client_len);  //create connection socket when host connects.
				//printf("rec");
				//receive first packet 
				read (connfd, buff, sizeof (buff));
				memcpy(init.mac, buff, sizeof(init.mac));
				memcpy(&(init.battery_level), buff+sizeof(init.mac), sizeof(init.battery_level));
				memcpy(&(init.test_codes), buff+sizeof(init.mac)+sizeof(init.battery_level), sizeof(init.test_codes));
				memcpy(&(init.version_major), buff+sizeof(init.mac)+sizeof(init.battery_level)+sizeof(init.test_codes), sizeof(init.version_major));
				memcpy(&(init.version_minor), buff+sizeof(init.mac)+sizeof(init.battery_level)+sizeof(init.test_codes)+sizeof(init.version_major), sizeof(init.version_minor));
				inet_ntop(AF_INET, &(client_addr.sin_addr), addr_buf, sizeof(addr_buf));

				uint8_t found = 0;
				uint8_t index = 0;
				//check if the mac is in our LUT
				for(int z = 0; z < 300; z++)
				{
					//bodge
					if( (init.mac[0] == macs[z][0]) \
					  &&(init.mac[1] == macs[z][1]) \
					  &&(init.mac[2] == macs[z][2]) \
					  &&(init.mac[3] == macs[z][3]) \
					  &&(init.mac[4] == macs[z][4]) \
					  &&(init.mac[5] == macs[z][5]))
					{
						found = 1;
						index = z;
						break;
					}
				}
				
				if(!found)
				{
					//add the ip and mac to out simple LUT
					memcpy(ips[n_connected], addr_buf, sizeof(addr_buf));
					memcpy(macs[n_connected], (init.mac), sizeof(init.mac));
					
					time(&rawtimes[n_connected]);
					battery_levels[n_connected] = init.battery_level;
					self_test_statuses[n_connected] = init.test_codes;
					uint16_t aport = assign_port();
					//printf("port assigned is %d, going in index %d", aport, n_connected);
					ports[n_connected] = aport;
					response.port = ports[n_connected];
					if(arguments.calibration == 1)
					{
						response.calibrate = 1;
						memcpy(t_args.mac, (init.mac), sizeof(init.mac)); //copy the mac into the cali thread args
					}
					else
						response.calibrate = 0;
					n_connected++;
				}
				else //found it already
				{
					//possible that we would need to update the ip
					memcpy(ips[index], addr_buf, sizeof(addr_buf));
					//printf("bleh");
					time(&rawtimes[index]);
					battery_levels[index] = (int)init.battery_level;
					self_test_statuses[index] = init.test_codes;
					response.port = ports[index];
					response.calibrate = 0;
					if(timer_started == 1)
						n_ready++; //find num ready to test
				}

				response.switch_network = check_switch_net();
				response.send_to_hibernate = is_testseq_init();
				response.test_begin = !(is_testseq_init());
				//response.init_self_test_proc = check_need_selftest(init.test_codes); //changed to calibrate command
				response.wireless = set_wireless_modes();

				memcpy(sendbuffer, &(response.fetch_update_command), 1);
				memcpy(sendbuffer+1, &(response.send_to_hibernate), 1);
				memcpy(sendbuffer+2, &(response.switch_network), 1);
				memcpy(sendbuffer+3, &(response.test_begin), 1);
				memcpy(sendbuffer+4, &(response.calibrate), 1);
				memcpy(sendbuffer+5, &(response.wireless), 1);
				memcpy(sendbuffer+6, &(response.port), 2);
				int x = write(connfd, sendbuffer, sizeof(sendbuffer));
				if(x != sizeof(sendbuffer))
					fprintf(stderr,"partial write");
				
				//need to spawn a thread to wait and recieve the calibration data.
				close(connfd);	//close connection socket once write has finished
			}
			else
			{
				//no new connections, just check mode
				if(mode == hibernate) //go back to top
					continue;
				else if(mode == initializing) //heres where we want to start the timer
				{	
					if(timer_started == 0)
					{
						//start the timer.
						//we will watch the timer for 12 mins to allow for the devices to connect for sure
						time(&timer_begin_time);
						timer_started = 1;
					}
					else
					{
						time(&timer_now);
						double timediff = difftime(timer_now, timer_begin_time);
						//if the number of devices is correct, proceed to test!
						if(n_connected == n_ready)
						{
							//still sleep to let the ntp go through
							sleep(120);
							break;
						}
						if(timediff >= 720) //720 sec is 12 min - allows for boards to connect to wifi as well as complete the ntp time sync.
						{
							/*
							//at least 1 device has not connected properly.
							//first, we need to find which one it is - whatever ones have times that are greater than 720 sec
							for(int x = 0; x < n_connected; x++)
							{
								double timediff_device = difftime(timer_now, rawtimes[x]);
								if(timediff_device >=720)
								{
									//this board has an error
								}
							}
							//then, let the user know
							//set an error flag so the print thread can show the error boards
							device_err_flag = 1;
							
							//jump into a while loop to wait for the user to take an action
							while(1)
							{
								//check if the flag has been unset
								if(device_err_flag == 0)
									break;
								sleep(1);
							}
							
							//if we get out of the loop, begin listening again and reset the timer
							
							timer_started = 0; //this will give us another bit of time for the board to reconnect
							continue;
							*/
						}
								
					}
				}
				else //something went wrong
					return -1;
			}
		}
		
		//we got here, we are now ready to begin the test sequence
		//TODO - only spawn extra threads if more than 1,2,3,4 devices connected
		
		//spawn the UDP threads to let them each spin up
		pthread_t rec1, rec2, rec3, rec4; //4 threads for 4 cores
		//thread_args_t arg1, arg2, arg3, arg4;
		int thread2_started = 0;
		int thread3_started = 0;
		int thread4_started = 0;
		// in the future we should be able to make the number of reciveing processes equal to number
		//of cores as a runtime parameter
		uint16_t port1 = 50001;
		uint16_t port2 = 50002;
		uint16_t  port3 = 50003;
		uint16_t  port4 = 50004;
		
		//set these based on the mode file
		//arg1.port = port1;
		//arg1.selected = 1;
		//arg1.selected_num = 1;
		
		pthread_create(&rec1, NULL, wasp_recieve, &port1);
		if(n_connected >=2)
		{
			pthread_create(&rec2, NULL, wasp_recieve, &port2);
			thread2_started = 1;
		}
		if(n_connected >= 3)
		{
			pthread_create(&rec3, NULL, wasp_recieve, &port3);
			thread3_started = 1;
		}
		if(n_connected >= 4)
		{
			pthread_create(&rec4, NULL, wasp_recieve, &port4);
			thread4_started = 1;
		}
		
		sleep(3); //wait for threads to enter their loops
		start_test(); //blast broadcast message 
		
		//start the async tcp
		//right now we prob just want to send a wasp packet to saying stop the test
		//and go back to hib
		pthread_t tcp_async;
		pthread_create(&tcp_async, NULL, tcp_async_command, NULL);
		
		
		pthread_join(rec1, NULL); //dont return from main until we are done!!!
		if(thread2_started)
			pthread_join(rec2, NULL); //dont return from main until we are done!!!
		if(thread3_started)
			pthread_join(rec3, NULL); //dont return from main until we are done!!!
		if(thread4_started)
			pthread_join(rec4, NULL); //dont return from main until we are done!!!
		
		pthread_join(tcp_async, NULL); //let the async server return so we know that the boards have been sent to sleep
		return 0;
}

void *tcp_async_command(void *arg)
{
	//if we got here, the test has started.
	//monitor the mode to see if it goes back to hibernate
	//if so, send a wasp command packet to send tht ebaord to sleep
	
	//the wasp board tcp serve doesnt check the content so what we send is irrelvant
	//just send a small buffer
	
	//tcp setup
	int i;
	int sockfd = 0;
	int n = 0;
	char buff[10];
	struct sockaddr_in serv_addr;
	memset(buff, '0', sizeof(buff));
	sprintf(buff, "HIBERNATE"); //cheeky little message
	memset(&serv_addr, '0', sizeof(serv_addr));
	
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket");
		exit(-1);
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons (60005); //wasp tcp serve listens at 60005
	
	FILE *modefile;
	enum testmode cur_mode;
	int printed = 0;
	
	while(1)
	{
		//check the mode
		modefile = fopen("mode", "rb");
		fscanf(modefile, "%d", &cur_mode);
		fclose(modefile);
		
		if(mode == hibernate)
		{
			if(!printed)
			{
				//let the print thread know with a flag here
				printf("test done, sending boards back to sleep\n");
				//have this in the print funct
				printed = 1;
			}
			
			//if we are here the user has switched the mode back to hibernate
			//so we want to send ALL the connected boards to sleep.
			
			for(int j = 0; j < n_connected; j++)
			{
				//iterate thorugh all the connected boards to send the sleep command
				
				//turn the ip strings into proper binary adresses
				if (inet_pton (AF_INET, ips[j], &serv_addr.sin_addr) <= 0)
				{
					perror("inet_pton");
					exit(-1);
				}
				
				//connect to the server
				//potientially this wont work when called second time - check this 
				if (connect (sockfd, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) < 0)
				{
					perror("connect");
					exit(-1);
				}
				
				//send the sleep message
				write(sockfd, buff, sizeof(buff));
				//move on to the next connected board
			}
			
			//if we got here, that means that all the baords have been sent to sleep
			//close the socket
			close(sockfd);
			
			//we are done, so we can return to the main thread to exit
			return NULL;
		}
		
		sleep(5);
	}
}

void start_test(void)
{
	int broadcast_sock;
	struct sockaddr_in broadcast_addr;
	char sendbuffer[10] = "TEST BEGIN";

	mode = testing;
	//write the mode to the file
	FILE *modefile;
	modefile = fopen("mode", "wb");
	fprintf(modefile, "%d", mode);
	fclose(modefile);

	if ((broadcast_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) //explicitly specify udp
	{
		perror("socket() failed");
	}

	//need to set permission to braodcast on linux
	int broadcastPermission = 1;
	if (setsockopt(broadcast_sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0)
	{
		perror("setsockopt() failed");
	}

	memset(&broadcast_addr, 0, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
	broadcast_addr.sin_port = htons(50006); //where wasp is listening

	for(int i = 0; i < 10; i++)
	{
		int ret = sendto(broadcast_sock, sendbuffer, sizeof(sendbuffer), 0, (struct sockaddr *) &broadcast_addr, sizeof(broadcast_addr));
		if(ret == -1)
		{
			perror("sendto() sent a different number of bytes than expected");
		}
	}
}

void *wasp_recieve(void *arg)
{
	uint16_t port = *(uint16_t *)arg; //the port this thread is responsible for
	threads_running++; //let the print know we are running
	
	//making local copies of these so there is less to search through
	char macs_local[75][6];
	char ips_local[75][14];
	int packet_counts_local[75]; //should def init to zero
	int global_indexes[75];
	
	//file pointers to each file - parallel list ot above two arrays
	//FILE *outputfiles[75];
	//choosing to stray from this - i dont want to keep the file pointers open in case of an error or something
	//if i just do the filenames, I can open and close and the data will be good even with an abrubt exit
	
	//00:a0:50:c4:26:a1
	char macaddrstr[18];
	char filename[21];
	char filenames[75][21]; //no longer needed
	FILE *outfiles[75];
	int num_devices = 0;
	//open all the files and write the csv headers
	//printf("%d, %d\n", ports[0], port); // was getting weird neg numbers, make sure ports are unsigned
	for(int i = 0; i <  300; i++) //loop through all the macs to find which ones we are assigned
	{
	
		if(ports[i] == port)
		{
			//this is one we are responsible for
			
			//turn the mac into a string
			sprintf(macaddrstr, "%02x:%02x:%02x:%02x:%02x:%02x", macs[i][0],macs[i][1],macs[i][2],macs[i][3],macs[i][4],macs[i][5]);
			sprintf(filename, "%s.csv", macaddrstr); //create the filename
			//printf("making a file called %s", filename);
			char *fullpath = (char*) malloc((strlen(datadir)+21)*sizeof(char));
			sprintf(fullpath, "%s%s", datadir, filename);
			
			//add the mac and ip to our local lists
			memcpy(macs_local[num_devices], macs[i], sizeof(macs[i]));
			memcpy(ips_local[num_devices], ips[i], sizeof(ips[i]));
			memcpy(filenames[num_devices], filename, sizeof(filename)); //no longer needed
			global_indexes[num_devices] = i;
			
			//open the file and print the csv header
			outfiles[num_devices] = fopen(fullpath, "wb");
			fprintf(outfiles[num_devices], "count,isotime,nanotime,samples\n");
			num_devices++;
			thread_devices_found++;
			
			//open the file and print the csv header
			
			
			//fclose(outfile); 
			//we wont be closing the files at each packet rx becuase of the performace penalty
			//we will hold them open and close all of them at the end of mode goes back to hib
			
		}
	}
	//all ready to rock!
	
	//udp stuff
	int udp_sock;
	int nBytes;
	char buf[sizeof(WASP_packet_t)];
	WASP_packet_t temp;
	struct sockaddr_in serverAddr, clientAddr;
	socklen_t client_len;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size, client_addr_size;
	int i;
	
	//standard socket intialization
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons ((short)port);
	serverAddr.sin_addr.s_addr = htonl (INADDR_ANY);
	memset ((char *)serverAddr.sin_zero, '\0', sizeof (serverAddr.sin_zero));  
	memset(&clientAddr, '\0', sizeof(clientAddr));
	addr_size = sizeof (struct sockaddr_in);
	
	if ((udp_sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf ("socket error - cant allocate sock 1\n");
		//return -1; //what to do instead
	}
	if (bind (udp_sock, (struct sockaddr *)&serverAddr, sizeof (serverAddr)) != 0)
	{
		printf ("bind error for sock 1\n");
		//return -1;
	}
	
	int len = 0;
	uint32_t count;
	iso8601_time_t tstart;
	memset(&tstart, '0', sizeof(tstart));
	uint64_t ntstart;
	uint16_t samps[236];
	char newline = '\n';
	char addr_str[INET_ADDRSTRLEN];
	int index;
	FILE *outfile;
	//wait for the test to begin
	
	//needs to be changed so it writes to the proper output file
	while(1)
	{
		//recvfrom is a blocking call - it wont return until a packet comes in 
		nBytes = recvfrom (udp_sock, buf, sizeof(buf), 0, (struct sockaddr *)&clientAddr, &addr_size);	
		inet_ntop(AF_INET, &(clientAddr.sin_addr), addr_str, sizeof(addr_str)); //get printable address
		//ip is how we look up the mac
		
		//find the ip in our table of ips
		for(int j = 0; j < num_devices; j++)
		{
			//printf("%s, %s", ips_local[j],addr_str);
			if(strcmp(ips_local[j],addr_str) == 0) //found it
			{
				index = j;
				break;
			}
			else
			{
				//something weird happened
				printf("got packet from unknown device on port %d\n", port);
				exit(-1);
				
			}
		}
		
		//grab the goods
		memcpy(&count, buf, sizeof(count));
		memcpy(&tstart, buf+sizeof(count), sizeof(tstart));
		memcpy(&ntstart, buf+sizeof(count)+sizeof(tstart), sizeof(ntstart));
		memcpy(samps, buf+sizeof(count)+sizeof(tstart)+sizeof(ntstart), sizeof(samps)); //put packet data in buffer all at once

		//choose the correct file to write to
		//outfile = outfiles[index];
		fprintf(outfiles[index], "%d,", count+1);
		fprintf(outfiles[index], "%s,", (char*)&tstart);
		fprintf(outfiles[index], "%f,", (float)ntstart);
		for(int i = 0; i < 236; i++)
			fprintf(outfiles[index], "%d ", samps[i]); //no comma between samples, all 1 column
		fprintf(outfiles[index], "\n");
		//fclose(outfile);
		fflush(outfiles[index]); //same as writing in that it writes buffered shit to file, but doesnt close the file!

		packet_counts_local[index]++;
		if(packet_counts_local[index]==10000)
		{
			packet_counts_local[index]=0;
			packet_counts[global_indexes[index]]++;
		}
		
		//check the mode
		if(mode == hibernate)
		{
			printf ("closing...\n");
			break;
		}
	}
	
	//close all the files to finalize the data
	for(int i = 0; i <  num_devices; i++)
	{
		fclose(outfiles[i]); 
	}
	
	return NULL;
}

void *print_data(void *arg) //numprint is num connected clients
{
	//time vars
	struct tm info;
	time_t time_now;
	char buffer[6];
	int num_rows = 0;
	double timediff;
	int timediff_int;
	int minutes;
	int seconds;
	int loopcount = 0;
	char macbuffer[18];
	
	FILE *modefile;
	enum testmode lastmode;
	//enum testmode mode; //using global mode
	
	
	while(1)
	{
		//erase old table
		for(int i = 0; i < num_rows+1; i++)
		{
			//erase the line
			printf("\33[2K");
			//move cursor up
			printf("\033[A");
		}
		num_rows = 0; //reset the current number of rows

		printf("\r"); //reset cursor to beginning of the row
		
		//find mode from mode file
		modefile = fopen("mode", "rb");
		fscanf(modefile, "%d", &mode);
		fclose(modefile);
		
		//print the mode
		if(mode == hibernate)
			printf("Current Mode = %-27s\n", "Hibernate");
		else if(mode == initializing)
			printf("Current Mode = %-27s\n", "Initializing Test Sequence");
		else if(mode == testing)
			printf("Current Mode = %-27s\n", "Test Active");
		else
		{
			//keeps last mode if not properly set
			mode = lastmode;
		}
		lastmode = mode;
		
		//print the number of devices on/awaiting a test
		printf("Devices ready to test = %-3d\n", n_ready);
		num_rows++;
		
		//print some debug info (hopefully this wont stay after all the kinks get worked out)
		printf("most recently assigned port: %d\n", ports[n_connected]);
		num_rows++;
		
		printf("Threads per device: %3d, %3d, %3d, %3d\n", thread_devices_found, 0, 0, 0);
		num_rows++;
		
		//header print
		if(mode != testing)
			printf("|Client  |MAC Address        |Battery Level %|Self-Test  |Time Since Ping |\n");
		else
			printf("|Client  |MAC Address        |Battery Level %|Self-Test  |Packets x10000  |\n"); //could aslo do 100000 in the space
		num_rows++;
		
		
		//print data for each connected client
		for(int j = 0; j < n_connected; j++)
		{
			//do the prints
			printf("|%-8d|", j); //client num - the index
			//need to turn mac into a string
			printf("%02x:", macs[j][0]); 
			printf("%02x:", macs[j][1]);
			printf("%02x:", macs[j][2]);
			printf("%02x:", macs[j][3]);
			printf("%02x:", macs[j][4]);
			printf("%02x  |", macs[j][5]);
			printf("%-15d|", battery_levels[j]); //batt level - 15 
			if( (self_test_statuses[j] == 7) || (self_test_statuses[j] == 6))
			{
				//could be 6 or 7, both are pass
				//diff is adc reset didnt get right arg back from the transfer
				//we dont care as long as self test passed and high z got set.
				printf("%-11s|", "PASS"); //st - 11 spaces
			}
			else
				printf("%-11s|", "FAIL"); //st - 11 spaces
			
			if(mode != testing)
			{
				//calculate the time stuff
				time(&time_now);
				timediff = difftime(time_now, rawtimes[j]);
				//turn delta into hrs and seconds
				timediff_int = (int)timediff;
				minutes = timediff_int/60;
				seconds = timediff_int%60;
				info.tm_sec = seconds;
				info.tm_min = minutes;
				strftime(buffer, 6, "%M:%S", &info);
				printf("%-16s|", buffer);
			}
			else
				printf("%-16d|", packet_counts[n_connected]);
			printf("\n");
			num_rows++;
		}
		loopcount++;
		sleep(5); //sleep 5 seconds between printing
	}
}
//replacing console with a lockfile like device for setting mode

int check_update(int major, int minor)
{
	
	return 0;
	/*
	//look for a file on the filesystem indicating the version of wasp currently avail on ota server
	FILE *versionfile;
	//versionfile = fopen("/mnt/usb/OTA/OTA_version.txt", "r"); //openwrt with edited fstab
	if((versionfile = fopen("/media/pi/DE96-17E5/OTA_version.txt", "r")) == NULL) //pi test server location
	{
		printf("unable to open version file");
		return 0; //0 means dont update anything
	}
	char buf[4]; //size of the line = 4 {1.1\n} - 4 chars
	fgets(buf, sizeof(buf), versionfile);
	//atoi expects a string - UGH need an intermediate buffer
	char interbuf[2] = {'\0', '\0'};
	interbuf[0] = buf[0];
	int avail_major = atoi(interbuf);

	interbuf[0] = buf[2];
	int avail_minor = atoi(interbuf);
	
	//version check
	if(avail_major > major) // ie 2.X > 1.x
		return 1; //grab update
	//major is probably equal to avail major
	else if(avail_major == major)
	{
		if(avail_minor > minor)
			return 1;
		else
			return 0;
	}
	else
		return -1; //something weird happened - device reporting higher major than avail for ota.
	*/
}

int is_testseq_init(void)
{
	//checks whether we have begun to prep for test. if we arent prepping, send device to hib
	if(mode == hibernate)
		return 1; //send to hibernate
	else
		return 0;
}

int check_switch_net(void)
{
	//ok so need to ask dezfouli about when to switch networks.
	//i can add in logic to see if a lot of packets are getting dropped, switch based on that?
	//more informed action in place later
	
	return 0;
	
	//for a start tho, I would say assign equla numbers to both, round robin style
	//assume that all registration messages come on 2.4ghz
	/*
	static int flag = 1;
	flag  = flag?0:1;
	return flag;
	*/
}

int check_need_selftest(uint16_t testcode) //deprecated
{
	//based on the values in the testcode field we would have logic for setting the self test procedure.
	//since we havent implemented that quite yet, just return 0 (ignore).
	return 0;
}

int set_wireless_modes(void)
{
	//need some fancy logic here. for now we will just set to random val becuase nothing is implemented client side
	return 122;
}

uint16_t assign_port(void)
{
	uint16_t valid_udp_ports[4] = {50001,50002,50003,50004};
	static int index = 4;
	//increment index
	if(index == 4)
		index = 0;
	else
		index++;
	return valid_udp_ports[index];
	
}

void disqualify(void)
{
	//this is the basis for a test disqualification
	//to be accurate, we will need a test duration
	//we will assume 20 min, but this should be a test operator argument
	//we will also need the battery size in milliamps
	//TODO - make the battery level an arg
	//TODO make the return val a bool
	//TODO if adding 5g, need to check if the device is on 5g
	
	int battery_level = 87;
	int battery_mah = 2500;
	int test_time_min = 20;
	
	float battery_percent = battery_level/100;
	float cur_mah = battery_percent*battery_mah;
	
	//we want to get the test time 
	float ma_2pt4g = 443.12;
	float ma_5g = 347.97;
	
	float hrs_2pt4g = cur_mah/ma_2pt4g;
	float hrs_5g = cur_mah/ma_5g;
	
	float min_2pt4g = hrs_2pt4g*60;
	float min_5g = hrs_5g*60;
	
	if( (min_2pt4g > test_time_min) || (min_5g > test_time_min))
	{
		return; //return true
	}
	
	return; //return false
}

//parser function, param order fixed by argp
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;
		case 'c':
			arguments->calibration = 1;
			break;
		case 'd':
			arguments->datadir = arg;
			break;
		case 'n':
			arguments->niface = arg;
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	
	return 0;
}