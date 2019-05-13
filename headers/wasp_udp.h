//what is a good sync time? ill choose 10 min?
#define TIME_SYNC_PERIOD    (10 * 60 * SECONDS)

//#define SERVER_IP_ADDRESS MAKE_IPV4_ADDRESS(192,168,1,219) //irrelvant
#define UDP_MAX_DATA_LENGTH         30 //this is a required argument to a function but it isnt actually used.

#define UDP_BROADCAST_RX_PORT 50006

extern wiced_udp_socket_t  udp_socket;
extern wiced_udp_socket_t  udp_broadcast_rx_sock;

typedef struct{
    uint32_t                packet_count; //udp -> including a packet count for easier reconstruction
    wiced_iso8601_time_t    time_start;
    uint64_t                nano_time_start;
    uint16_t                samples[236];
}wasp_pckt_t; //4 + 236*2 + 27 + 8 bytes = 511 bytes

wiced_result_t init_udp(int port);
wiced_result_t send_wasp_packets(void* assigned_port);
wiced_result_t rx_udp(); //wait for test start message
uint16_t random_sample(void); //leaving in case we still want to send dummy packets somewhere

//these are here solely for the purpose of server debug tests, not intended to be used elsewhere
wiced_result_t tx_udp_packet_known_good();
wiced_result_t tx_udp_packet();
wiced_result_t package_sample();
wiced_result_t send(wasp_pckt_t pckt);
