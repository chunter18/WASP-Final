#include "wiced.h"
#include "../headers/wasp_tcp.h"
#include "../headers/wasp_network_global.h"
#include "../headers/wasp_hibernate.h"

wiced_tcp_socket_t  tcp_client_socket;
wiced_tcp_socket_t  tcp_server_socket;
uint8_t test_concluded = 0;

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

wiced_result_t tcp_server_start_async(void)
{
    wiced_result_t result;
    if ( wiced_tcp_create_socket( &tcp_server_socket, WICED_STA_INTERFACE ) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ("TCP socket creation failed\r\n") );
        return WICED_ERROR;
    }

    /* Register callbacks to handle various TCP events */
    result = wiced_tcp_register_callbacks( &tcp_server_socket, client_connected_callback, received_data_callback, client_disconnected_callback, NULL );

    if ( result != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ("TCP server socket initialization failed\r\n") );
        return WICED_ERROR;
    }

    /* Start TCP server to listen for connections */
    if ( wiced_tcp_listen( &tcp_server_socket, TCP_ASYNC_PORT ) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ("TCP server socket initialization failed\r\n") );
        wiced_tcp_delete_socket( &tcp_server_socket );
        return WICED_ERROR;
    }
    return WICED_SUCCESS;
}

wiced_result_t client_connected_callback( wiced_tcp_socket_t* socket, void* arg )
{
   wiced_result_t      result;

   UNUSED_PARAMETER( arg );

   /* Accept connection request */
   result = wiced_tcp_accept( socket );
   return result;
}
wiced_result_t client_disconnected_callback( wiced_tcp_socket_t* socket, void* arg )
{
    UNUSED_PARAMETER( arg );

    wiced_tcp_disconnect(socket);

    /* Start listening on the socket again */
    if ( wiced_tcp_listen( socket, TCP_ASYNC_PORT ) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ("TCP server socket re-initialization failed\r\n") );
        wiced_tcp_delete_socket( socket );
        return WICED_ERROR;
    }

    return WICED_SUCCESS;
}
wiced_result_t received_data_callback( wiced_tcp_socket_t* socket, void* arg )
{
    wiced_result_t      result;
    wiced_packet_t*     tx_packet;
    char*               tx_data;
    wiced_packet_t*     rx_packet = NULL;
    char*               request;
    uint16_t            request_length;
    uint16_t            available_data_length;

    result = wiced_tcp_receive( socket, &rx_packet, WICED_WAIT_FOREVER );
    if ( result != WICED_SUCCESS )
    {
        return result;
    }

    wiced_packet_get_data( rx_packet, 0, (uint8_t**) &request, &request_length, &available_data_length );

    //get data
    //for now the data doesnt matter - only case where we get this is if we are sending to hib
    //if a user wants more we can easily add here, but otherwise just send to hib.
    test_concluded = 1;

    /* Release a packet */
    wiced_packet_delete( rx_packet );

    //send to hib
    send_to_hibernate();

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
    resp.calibrate = rx_data[4];
    resp.wireless = rx_data[5];
    resp.port = ((uint16_t)rx_data[7] << 8) | rx_data[6]; //these are reversed because network order is reversed


    /* Delete the packet */
    wiced_packet_delete(rx_packet);

    //this terminates the connection.
    //wiced_tcp_disconnect(&tcp_client_socket);


    return resp;
}

wiced_result_t send_calibration_data(float cal_data)
{
    wiced_result_t           result;
    wiced_packet_t*          packet;
    char*                    tx_data;
    uint16_t                 available_data_length;

    /* Create the TCP packet. Memory for the tx_data is automatically allocated */
    if (wiced_packet_create_tcp(&tcp_client_socket, 30, &packet, (uint8_t**)&tx_data, &available_data_length) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("TCP packet creation failed\n"));
        return WICED_ERROR;
    }

    /* Write the message into the packet*/
    memcpy(tx_data, &cal_data, sizeof(cal_data));

    /* Set the end of the data portion */
    wiced_packet_set_data_end(packet, (uint8_t*)tx_data + sizeof(cal_data));

    /* Send the TCP packet */
    if (wiced_tcp_send_packet(&tcp_client_socket, packet) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("Failed to send calibration packet to the server.\n"));

        /* Delete packet, since the send failed */
        wiced_packet_delete(packet);

        /* Close the connection */
        wiced_tcp_disconnect(&tcp_client_socket);
        return WICED_ERROR;
    }

    //wiced_tcp_disconnect(&tcp_client_socket);

    return WICED_SUCCESS;

}


