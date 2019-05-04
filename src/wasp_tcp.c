#include "wiced.h"
#include "../headers/wasp_tcp.h"
#include "../headers/wasp_network_global.h"

wiced_tcp_socket_t  tcp_client_socket;
//making these constant so we dont need to calc them each time
size_t macsize = 6;     //sizeof(init_packet_t.mac);
size_t battsize = 1;    //sizeof(init_packet_t.battery_level);
size_t tstsize = 2;     //sizeof(send_packet.test_codes);
size_t mjsize =  1;     //sizeof(send_packet.version_major);
size_t mrsize =  1;     //sizeof(send_packet.version_minor);
size_t total = 11;      //macsize+battsize+tstsize+mjsize+mrsize;

wiced_result_t tcp_init(void)
{
    /* Create a TCP socket */
    if ( wiced_tcp_create_socket( &tcp_client_socket, WICED_STA_INTERFACE ) != WICED_SUCCESS ) //work
    {
        WPRINT_APP_INFO( ("TCP socket creation failed\n") );
        return WICED_ERROR;
    }

    /* Bind to the socket */
    wiced_tcp_bind( &tcp_client_socket, TCP_SERVER_PORT );

    return WICED_SUCCESS;
}

wiced_mac_t get_mac(void)
{
    wiced_mac_t                     original_mac_address;
    platform_dct_wifi_config_t*     dct_wifi_config          = NULL;

    /* get the wi-fi config section for modifying, any memory allocation required would be done inside wiced_dct_read_lock() */
    wiced_dct_read_lock( (void**) &dct_wifi_config, WICED_TRUE, DCT_WIFI_CONFIG_SECTION, 0, sizeof( *dct_wifi_config ) );


   original_mac_address = dct_wifi_config->mac_address;

   //need to release the read lock
   wiced_dct_read_unlock( dct_wifi_config, WICED_TRUE );

   return original_mac_address;
}

wiced_result_t tcp_connect(void)
{
    return WICED_SUCCESS;
}
init_response_t register_with_server(init_packet_t send_packet)
{
    wiced_result_t           result;
    wiced_packet_t*          packet;
    wiced_packet_t*          rx_packet;
    char*                    tx_data;
    char*                    rx_data;
    uint16_t                 rx_data_length;
    uint16_t                 available_data_length;
    init_response_t          resp;
    const wiced_ip_address_t INITIALISER_IPV4_ADDRESS( server_ip_address, TCP_SERVER_IP_ADDRESS );

    resp.port = 0; //this is our return error condition. if we dont get assigned a port, something went wrong.

    /* Connect to the remote TCP server, try several times */
    int connection_retries = 0;
    do
    {
        result = wiced_tcp_connect( &tcp_client_socket, &server_ip_address, TCP_SERVER_PORT, TCP_CLIENT_CONNECT_TIMEOUT );
        connection_retries++;
        wiced_rtos_delay_microseconds(15);
    }
    while( ( result != WICED_SUCCESS ) && ( connection_retries < TCP_CONNECTION_NUMBER_OF_RETRIES ) );
    if( result != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("Unable to connect to the server! Halt.\n"));
        return resp;
    }

    /* Create the TCP packet. Memory for the tx_data is automatically allocated */
    if (wiced_packet_create_tcp(&tcp_client_socket, 30, &packet, (uint8_t**)&tx_data, &available_data_length) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("TCP packet creation failed\n"));
        return resp;
    }


    /* Write the message into the packet*/
    memcpy(tx_data, (send_packet.mac), macsize);
    memcpy(tx_data+macsize, &(send_packet.battery_level), battsize);
    memcpy(tx_data+macsize+battsize, &(send_packet.test_codes), tstsize);
    memcpy(tx_data+macsize+battsize+tstsize, &(send_packet.version_major), mjsize);
    memcpy(tx_data+macsize+battsize+tstsize+mjsize, &(send_packet.version_minor), mrsize);

    /* Set the end of the data portion */
    wiced_packet_set_data_end(packet, (uint8_t*)tx_data + total);

    /* Send the TCP packet */
    if (wiced_tcp_send_packet(&tcp_client_socket, packet) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("Failed to send registration packet to the server.\n"));

        /* Delete packet, since the send failed */
        wiced_packet_delete(packet);

        /* Close the connection */
        wiced_tcp_disconnect(&tcp_client_socket);
        return resp;
    }

    /* Receive a response from the server */
    result = wiced_tcp_receive(&tcp_client_socket, &rx_packet, TCP_CLIENT_RECEIVE_TIMEOUT);
    if( result != WICED_SUCCESS )
    {
        WPRINT_APP_INFO(("Couldnt recieve registration response from the server.\n"));

        /* Delete packet, since the receive failed */
        wiced_packet_delete(rx_packet);

        /* Close the connection */
        wiced_tcp_disconnect(&tcp_client_socket);
        return resp;
    }


    /* Get the contents of the received packet */
    wiced_packet_get_data(rx_packet, 0, (uint8_t**)&rx_data, &rx_data_length, &available_data_length);

    //fill the response fields
    resp.fetch_update_command = rx_data[0];
    resp.send_to_hibernate = rx_data[1];
    resp.switch_network = rx_data[2];
    resp.test_begin = rx_data[3];
    resp.init_self_test_proc = rx_data[4];
    resp.wireless = rx_data[5];
    resp.port = ((uint16_t)rx_data[7] << 8) | rx_data[6]; //these are reversed because network order is reversed


    /* Delete the packet */
    wiced_packet_delete(rx_packet);

    //this terminates the connection.
    //wiced_tcp_disconnect(&tcp_client_socket);


    return resp;
}



