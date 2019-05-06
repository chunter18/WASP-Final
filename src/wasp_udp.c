#include "wiced.h"
#include "sntp.h"
#include <inttypes.h>
#include "../headers/wasp_udp.h"
#include "../headers/wasp_tcp.h"
#include "../headers/wasp_network_global.h"
#include "../headers/wasp_threads.h"


wiced_udp_socket_t  udp_socket;
static uint32_t tx_count   = 0; //not needed i dont think

//like tcp, making these lengths constant so we dont calc them every time.
size_t ctsize = 4;       //sizeof(raw_data.packet_count);
size_t isosize = 27;      //sizeof(raw_data.time_start);
size_t nsize = 8;        //sizeof(raw_data.nano_time_start);
size_t ssize = 472;        //sizeof(raw_data.samples);
size_t udptotal = 511;        //ctsize+isosize+nsize+ssize;

wiced_result_t init_udp(int port)
{
    /* Create UDP socket */
    if (wiced_udp_create_socket(&udp_socket, port, WICED_STA_INTERFACE) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO( ("UDP socket creation failed\n") );
        return WICED_ERROR;
    }
    return WICED_SUCCESS;
}

wiced_result_t send_wasp_packets(void* assigned_port)
{
    //get port as thread argument
    uint16_t *portptr = (uint16_t*)(assigned_port);
    uint16_t port = *portptr;

    //count is derived from comm queue message

    wiced_packet_t*          packet;
    char*                    udp_data;
    wasp_pckt_t              raw_data;
    uint16_t                 available_data_length;
    const wiced_ip_address_t INITIALISER_IPV4_ADDRESS( target_ip_addr, SERVER_IP_ADDRESS );
    uint16_t                 sample1;
    uint16_t                 sample2;
    uint32_t                 x;
    uint32_t                 lowmask = 0x0000FFFF;
    uint32_t                 highmask = 0xFFFF0000;
    wiced_result_t           queuefull;
    uint64_t                 command = 0; //first three times this gets run well get garbage data, dont send those
    uint32_t                 pckt_count;
    uint32_t                 sendbool;


    while(!test_concluded)
    {
        queuefull = wiced_rtos_is_queue_full(&comm_queue);
        if(queuefull == WICED_SUCCESS)
        {
            wiced_rtos_pop_from_queue(&comm_queue, &command, 0);
            //64 bit quantity comes off. upper 32 is packet count, lower 32 is send yes/no
            pckt_count = (uint32_t)(command >> 32);
            sendbool = (uint32_t)command; //gets lower 32

            if(sendbool)
            {
                /* Create the UDP packet */
                if ( wiced_packet_create_udp( &udp_socket, 512, &packet, (uint8_t**) &udp_data, &available_data_length ) != WICED_SUCCESS )
                {
                    WPRINT_APP_INFO( ("UDP tx packet creation failed\n") );
                    return WICED_ERROR;
                }

                /* packing the data into the packet */
                raw_data.packet_count = pckt_count;

                //probaby overfills, so need to do before samples.
                wiced_rtos_pop_from_queue(&iso_time_queue, &raw_data.time_start, 0);

                for(int i = 0; i < 236; i+=2)
                {
                   wiced_rtos_pop_from_queue(&main_queue, &x, 0);
                   sample2 = (uint16_t)(lowmask&x);
                   sample1 = (uint16_t)((highmask&x) >> 16);
                   //placed in queue with high 16 bits being first sample, low 16 being second
                   raw_data.samples[i] = sample1;
                   raw_data.samples[i+1] = sample2;
                }


                wiced_rtos_pop_from_queue(&nano_time_queue, &raw_data.nano_time_start, 0);

                memcpy(udp_data, &(raw_data.packet_count), ctsize);
                memcpy(udp_data+ctsize, &(raw_data.time_start), isosize);
                memcpy(udp_data+ctsize+isosize, &(raw_data.nano_time_start), nsize);
                memcpy(udp_data+ctsize+isosize+nsize, (raw_data.samples), ssize);

                /* Set the end of the data portion */
                wiced_packet_set_data_end( packet, (uint8_t*) udp_data + udptotal );

                /* Send the UDP packet */
                if ( wiced_udp_send( &udp_socket, &target_ip_addr, port, packet ) != WICED_SUCCESS )
                {
                    WPRINT_APP_INFO( ("UDP packet send failed\n") );
                    wiced_packet_delete( packet ); /* Delete packet, since the send failed */
                    return WICED_ERROR;
                }
            }
        }
    }

    return WICED_SUCCESS;
}
wiced_result_t rx_udp()
{
    //wait to recieve the broadcast message to start a test.
    wiced_packet_t* packet;
    char*           udp_data;
    uint16_t        data_length;
    uint16_t        available_data_length;
    uint32_t        timeout = 0xFFFFFFFF; //maximum timeout

    /* Wait for UDP packet */
    wiced_result_t result = wiced_udp_receive( &udp_socket, &packet, timeout );

    if ( ( result == WICED_ERROR ) || ( result == WICED_TIMEOUT ) )
    {
        return result;
    }

    wiced_packet_get_data( packet, 0, (uint8_t**) &udp_data, &data_length, &available_data_length );

    /*
    //in case we want to see the data in the future
    udp_data[ data_length ] = '\x0';
    WPRINT_APP_INFO( ("%s\n\n", udp_data) );
    */

    /* Delete packet as it is no longer needed */
    wiced_packet_delete( packet );


    return WICED_SUCCESS;
}
uint16_t random_sample(void)
{
    return (uint16_t)(rand() % 65535);
}
