#define TCP_SERVER_PORT                   50005
#define TCP_ASYNC_PORT                    60005
#define TCP_CLIENT_CONNECT_TIMEOUT        500
#define TCP_CLIENT_RECEIVE_TIMEOUT        500
#define TCP_CONNECTION_NUMBER_OF_RETRIES  30
#define TCP_PACKET_MAX_DATA_LENGTH        30 //why

typedef struct
{
    uint8_t mac[6];
    uint8_t battery_level;
    uint16_t test_codes;
    uint8_t version_major;
    uint8_t version_minor;
    //add a field to let server know the battery size as set during build
    //this will allow for us to support multiple batt sizes for our system, especially for the disqual algo
}init_packet_t;

typedef struct
{
    uint8_t fetch_update_command;
    uint8_t send_to_hibernate;
    uint8_t switch_network;
    uint8_t test_begin;
    uint8_t calibrate; //used to be more self tests (init_self_test_proc)
    uint8_t wireless; //placeholder for wireless mode stuff?
    //possibly have more self tests in a variable
    uint16_t port;
}init_response_t; //response message to client device registration

typedef struct
{
    uint8_t diagnostic_codes[4];
    uint8_t extras[4]; //not sure what we need yet
}control_ack_t;

extern wiced_tcp_socket_t  tcp_client_socket;
extern wiced_tcp_socket_t  tcp_server_socket;
extern uint8_t test_concluded;

wiced_result_t tcp_init(void);
wiced_result_t tcp_server_start_async(void);
wiced_result_t client_connected_callback( wiced_tcp_socket_t* socket, void* arg );
wiced_result_t client_disconnected_callback( wiced_tcp_socket_t* socket, void* arg );
wiced_result_t received_data_callback( wiced_tcp_socket_t* socket, void* arg );
wiced_mac_t get_mac(void);
wiced_result_t tcp_connect(void);
init_response_t register_with_server(init_packet_t send_packet);
wiced_result_t send_calibration_data(float data);
