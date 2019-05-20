//define the needed server IP address here.

#define SERVER_IP_ADDRESS MAKE_IPV4_ADDRESS(192,168,50,193)
#define SERVER_IP_STRING "192.168.1.219"
//tcp and udp will inhereit from the above set addr.
#define TCP_SERVER_IP_ADDRESS SERVER_IP_ADDRESS
#define UDP_SERVER_IP_ADDRESS SERVER_IP_ADDRESS

//the above become irrelevant once sdp is integrated
#define BROADCAST_IP MAKE_IPV4_ADDRESS(255,255,255,255)
