#include "wiced.h"
#include "sntp.h"
#include <inttypes.h>
#include "../headers/wasp_udp.h"
#include "../headers/wasp_tcp.h"
#include "../headers/wasp_network_global.h"
#include "../headers/wasp_threads.h"
#include "../headers/wasp_adxl.h"
#include "../headers/WASP_LED.h"
#include "wiced_osl.h"


wiced_udp_socket_t  udp_socket;
wiced_udp_socket_t  udp_broadcast_rx_sock;
//static uint32_t tx_count   = 0; //not needed i dont think

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
    if (wiced_udp_create_socket(&udp_broadcast_rx_sock, UDP_BROADCAST_RX_PORT, WICED_STA_INTERFACE) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO( ("UDP broadcast socket creation failed\n") );
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
            toggle_red_led();
            wiced_rtos_pop_from_queue(&comm_queue, &command, 0);
            //64 bit quantity comes off. upper 32 is packet count, lower 32 is send yes/no
            pckt_count = (uint32_t)(command >> 32);
            sendbool = (uint32_t)command; //gets lower 32

            /* packing the data into the packet */
            raw_data.packet_count = pckt_count;

            //probably overfills, so need to do before samples.
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

            if(sendbool)
            {
                /* Create the UDP packet */
                if ( wiced_packet_create_udp( &udp_socket, 512, &packet, (uint8_t**) &udp_data, &available_data_length ) != WICED_SUCCESS )
                {
                    WPRINT_APP_INFO( ("UDP tx packet creation failed\n") );
                    return WICED_ERROR;
                }

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
    wiced_result_t result = wiced_udp_receive( &udp_broadcast_rx_sock, &packet, timeout );

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

wiced_result_t tx_udp_packet_known_good()
{
    wiced_packet_t*          packet;
    char*                    udp_data;
    wasp_pckt_t              raw_data;
    uint16_t                 available_data_length;
    wiced_iso8601_time_t     iso8601_time;
    int tx_count = 0;
    uint16_t sample = 0;
    int16_t nsegments = 2;
    wiced_spi_message_segment_t message[1];
    uint8_t txbuf[2], rxbuf[2];
    txbuf[0] = 0xFF;
    txbuf[1] = 0xFF;
    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;
    message[0].length = 2;
    const wiced_ip_address_t INITIALISER_IPV4_ADDRESS( target_ip_addr, SERVER_IP_ADDRESS );

    while(1)
    {
        /* Create the UDP packet */
        if ( wiced_packet_create_udp( &udp_socket, 512, &packet, (uint8_t**) &udp_data, &available_data_length ) != WICED_SUCCESS )
        {
            WPRINT_APP_INFO( ("UDP tx packet creation failed\n") );
            return WICED_ERROR;
        }

        /* fiddling with the data */
        raw_data.packet_count = tx_count++;
        wiced_time_get_iso8601_time( &iso8601_time );
        raw_data.time_start = iso8601_time;
        raw_data.nano_time_start = wiced_get_nanosecond_clock_value();

        for(int i = 0; i < 236; i++)
        {
            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_low(WICED_GPIO_34);
            if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
            {
                //return WICED_ERROR;
                osl_udelay(1);
            }
            sample = ((uint16_t)rxbuf[0] << 8) | rxbuf[1];
            raw_data.samples[i] = sample;
        }

        memcpy(udp_data, &(raw_data.packet_count), ctsize);
        memcpy(udp_data+ctsize, &(raw_data.time_start), isosize);
        memcpy(udp_data+ctsize+isosize, &(raw_data.nano_time_start), nsize);
        memcpy(udp_data+ctsize+isosize+nsize, (raw_data.samples), ssize);

        /* Set the end of the data portion */
        wiced_packet_set_data_end( packet, (uint8_t*) udp_data + udptotal );

        /* Send the UDP packet */
        if ( wiced_udp_send( &udp_socket, &target_ip_addr, 50001, packet ) != WICED_SUCCESS )
        {
            WPRINT_APP_INFO( ("UDP packet send failed\n") );
            wiced_packet_delete( packet ); /* Delete packet, since the send failed */
            return WICED_ERROR;
        }

        wiced_watchdog_kick();
    }
    return WICED_SUCCESS;
}

wiced_result_t tx_udp_packet()
{
    wiced_packet_t*          packet;
    char*                    udp_data;
    wasp_pckt_t              raw_data;
    uint16_t                 available_data_length;
    wiced_iso8601_time_t     iso8601_time;
    int tx_count = 0;
    uint16_t sample = 0;
    int16_t nsegments = 2;
    wiced_spi_message_segment_t message[1];
    uint8_t txbuf[2], rxbuf[2];
    txbuf[0] = 0xFF;
    txbuf[1] = 0xFF;
    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;
    message[0].length = 2;
    wiced_thread_t shitter;

    while(1)
    {

        /* fiddling with the data */
        raw_data.packet_count = tx_count++;
        wiced_time_get_iso8601_time( &iso8601_time );
        raw_data.time_start = iso8601_time;
        raw_data.nano_time_start = wiced_get_nanosecond_clock_value();

        for(int i = 0; i < 236; i++)
        {
            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_low(WICED_GPIO_34);
            if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
            {
                //return WICED_ERROR;
                osl_udelay(1);
            }
            sample = ((uint16_t)rxbuf[0] << 8) | rxbuf[1];
            raw_data.samples[i] = sample;
        }

        send(raw_data);
        //wiced_rtos_create_thread (&shitter, 4, "shitter", send, 5*1024, NULL);
        wiced_watchdog_kick();
    }

    return WICED_SUCCESS;
}

wiced_result_t package_sample()
{

    wiced_result_t ret = tx_udp_packet();
    /*
    while(1)
    {
       wiced_result_t ret = tx_udp_packet();
       if (ret != WICED_SUCCESS)
           WPRINT_APP_INFO(("packet send failed\n"));
       wiced_watchdog_kick();
    }
    */
    return WICED_SUCCESS;

}

wiced_result_t send(wasp_pckt_t pckt)
{
    wiced_packet_t*          packet;
    char*                    udp_data;
    uint16_t                 available_data_length;
    const wiced_ip_address_t INITIALISER_IPV4_ADDRESS( target_ip_addr, SERVER_IP_ADDRESS );

    /* Create the UDP packet */
    if ( wiced_packet_create_udp( &udp_socket, 512, &packet, (uint8_t**) &udp_data, &available_data_length ) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ("UDP tx packet creation failed\n") );
        return WICED_ERROR;
    }

    memcpy(udp_data, &(pckt.packet_count), ctsize);
    memcpy(udp_data+ctsize, &(pckt.time_start), isosize);
    memcpy(udp_data+ctsize+isosize, &(pckt.nano_time_start), nsize);
    memcpy(udp_data+ctsize+isosize+nsize, (pckt.samples), ssize);

    /* Set the end of the data portion */
    wiced_packet_set_data_end( packet, (uint8_t*) udp_data + udptotal );

    /* Send the UDP packet */
    if ( wiced_udp_send( &udp_socket, &target_ip_addr, 50001, packet ) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ("UDP packet send failed\n") );
        wiced_packet_delete( packet ); /* Delete packet, since the send failed */
        return WICED_ERROR;
    }

    return WICED_SUCCESS;
}
