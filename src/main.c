#include "../headers/WASP.h"

void spi_startup(void *arg);
void sample_main(void *arg);

wiced_queue_t spi_starup_queue;
wiced_thread_t spi_sample_thread;
wiced_thread_t udp_network_thread;

void application_start( )
{
    /*
     * wasp boot sequence elaboration
     * check if its an ota boot
     * if yes - update
     * if no, continue
     *
     * check if we have just returned from hibernate
     * if no -
     *   bring up the gas guage for first boot
     * if yes -
     *   check battery level instead of initializing the gas guage
     *   proceed to rest of boot
     *
     *
     * normal boot
     * init the device
     * bring up the adc and prepare it for taking data
     * bring up network
     * register with server
     *      check to see if we should hibernate again
     *      if yes - go to hibernate
     *      if no - begin to wait
     *
     *
     * going to sleep -
     * set ota mode
     * undo adc status reg?
     * set dct 2.4 ghz?
     */

    int bootflag = is_ota_boot();
    if(bootflag)
    {
        WPRINT_APP_INFO( ( "OTA\n") ); //change this to OTA boot function when ready
        ota_update();
    }

    //no else needed, if not ota just keep on as per usual

    uint16_t prescaler = find_prescalar();
    wiced_result_t result;
    wiced_timer_t timer;
    uint8_t batt_level;
    init_response_t server_init_cmds;
    init_packet_t registration_pckt;
    wiced_mac_t hostmac;
    wiced_result_t res;

    int returned_from_hib = is_hib_wake();
    if(returned_from_hib) //we DID just return from hibernate, which means dont set
    {
        WPRINT_APP_INFO( ( "returned from hib\n") );
        wiced_rtos_delay_milliseconds(100); //do we need to wait for v ails here too?
        //check batt level
        batt_level = battery_percentage(prescaler);
    }
    else //first time boot, bring up gas guage
    {
        WPRINT_APP_INFO( ( "first boot.\n") );
        //delay to let the voltage rails come up
        wiced_rtos_delay_milliseconds(100);
        result = LTC2941_init_startup();
        if(result != WICED_SUCCESS)
        {
            //blink red only - gas gauage didnt come up correctly.
            result = blink(3, 500, &timer); //blink every half second
        }
        batt_level = battery_percentage(prescaler); //should be 100%
    }

    //need to get data back from spi init thread - use a queue
    //we will read the data and immediately deallocate the space.
    result = wiced_rtos_init_queue(&spi_starup_queue, NULL, 8, 1);

    //adc stuff - it exists in a different thread
    //ideally we would want this in the same thread, but it needs a bigger stack
    //and we cant seem to give the main thread a bigger stack through wiced_defaults.h

    wiced_thread_t spi_init_thread;
    result = wiced_rtos_create_thread (&spi_init_thread, 0, "", spi_startup, 10*1024, 0);
    wiced_rtos_thread_join(&spi_init_thread); //wait for it to finish
    uint32_t test_codes;
    wiced_rtos_pop_from_queue(&spi_starup_queue, &test_codes, 0);
    registration_pckt.test_codes = (uint16_t)test_codes;
    //dealloc the queue - not needed anymore
    wiced_rtos_deinit_queue(&spi_starup_queue);

    //gas guage is finished, do rest of intialization
    wiced_init(); //initialized the RTOS

    //bring up the network using last know config
    wiced_network_up(WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL);

    //create the TCP client to register with the server
    result = tcp_init();
    if(result != WICED_SUCCESS)
        WPRINT_APP_INFO( ( "TCP FAILED\n") );


    //fill the registration packet appropriately.
    hostmac = get_mac();
    memcpy(registration_pckt.mac, hostmac.octet, sizeof(registration_pckt.mac));
    registration_pckt.battery_level = batt_level;
    registration_pckt.version_major = 1;
    registration_pckt.version_minor = 0;


    float mah = LTC2941_get_mAh(prescaler);
    WPRINT_APP_INFO( ( "mah = %f\n", mah) ); //debug

    //register with server
    server_init_cmds = register_with_server(registration_pckt);

    //check to see if registration was an error
    if(server_init_cmds.port == 0)
    {
        //error, something went wrong
        WPRINT_APP_INFO( ( "ERROR PORT NOT ASSIGNED\n") );
        return;
    }


    //check whether to go back to sleep or not.
    if(server_init_cmds.send_to_hibernate == 1) //for test, should always be 1
    {
        WPRINT_APP_INFO( ( "SERVER SENDING TO HIB\n") );

        //should we disconnnect from tcp? we wont yet but maybe we should.

        //ota boot should still be zero - leave it be.

        //go back to hibernate for another 10 mins.
        send_to_hibernate();
    }

    //if we got past we got at least 1 command
    if(server_init_cmds.fetch_update_command == 1)
    {
        //set the ota params in dct and send to hib to reset

        //for now just going to use the server addr and a const name.
        //TODO - have the server send this in response packet.
        int n = sizeof(SERVER_IP_STRING);
        n += sizeof("/OTA2_image_file.bin");
        n--; //removes an extra null
        char *server = (char*) malloc((n)*sizeof(char));
        strcpy(server, SERVER_IP_STRING);
        strcpy(server, "/OTA2_image_file.bin"); //one these should be a memcpy
        send_to_OTA(server);
    }

    //if we got here, its just commands
    //we didnt get told to go to hib, so get ready to do the test while we can
    //switching networks and extra self tests are unimplemented as of yet

    uint16_t port = server_init_cmds.port;
    platform_init_nanosecond_clock();

    //create udp socket
    init_udp(port);
    WPRINT_APP_INFO( ( "created udp socket at port %d\n", port) );

    //sync with ntp
    WPRINT_APP_INFO( ( "starting time sync..\n") );
    sntp_start_auto_time_sync( TIME_SYNC_PERIOD );
    WPRINT_APP_INFO( ( "finished.\n") );

    //create queues and fill them like normal
    result = init_queues();
    WPRINT_APP_INFO( ( "queues ready.\n") );

    //wait for the test start broadcast
    result = rx_udp();
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ( "start test\n") );

    //spawn consumer thread
    result = wiced_rtos_create_thread (&udp_network_thread, 5, "udp", send_wasp_packets, 10*1024, &port);

    //spawn tcp async thread?
    //wiced_thread_t tcp_async_thread; does this need to be a thread??
    tcp_server_start_async();

    //spawn producer thread
    result = wiced_rtos_create_thread (&spi_sample_thread, 4, "spi", sample_main, 10*1024, 0);

    return;
}

void spi_startup(void *arg)
{
    UNUSED_PARAMETER(arg);

    wiced_result_t res;
    uint32_t testcodes = 0;
    uint8_t one = 1;

    res = adc_unset_high_z();
    if(res == WICED_SUCCESS)
        testcodes |= one;

    res = adxl_self_test();
    if(res == WICED_SUCCESS)
        testcodes |= (one<<1);

    res = adc_set_high_z();
    if(res == WICED_SUCCESS)
        testcodes |= (one<<2);

    wiced_rtos_push_to_queue(&spi_starup_queue, &testcodes, 0);
    return;
}

void sample_main(void *arg)
{
    UNUSED_PARAMETER(arg);
    uint64_t nano_test_start;
    uint64_t nano_test_end;
    uint64_t nano_time;
    wiced_iso8601_time_t iso8601_time;
    uint16_t sample1;
    uint16_t sample2;
    uint32_t message;
    uint32_t loopcount = 0;
    uint32_t pckt_count = 0;
    uint64_t command;
    uint32_t send = 0;

    WPRINT_APP_INFO( ( "begin sampling\n") );

    nano_test_start = wiced_get_nanosecond_clock_value(); //mark it just in case, nut test ends will be async

    while(!test_concluded)
    {
        //this isnt EXACTLY regular - TODO.
        wiced_time_get_iso8601_time( &iso8601_time );
        nano_time = wiced_get_nanosecond_clock_value();
        wiced_rtos_push_to_queue(&iso_time_queue, &iso8601_time, 0);
        wiced_rtos_push_to_queue(&nano_time_queue, &nano_time, 0);

        for(int i = 0; i < 118; i++)
        {
            sample1 = adc_sample(); //takes about 20us
            sample2 = adc_sample();
            message = ((uint32_t)sample1 << 16) | sample2;
            wiced_rtos_push_to_queue(&main_queue, &message, 0);
        }

        //create message for the other thread
        loopcount++;
        if(loopcount >= 3)
        {
            pckt_count++;
            send = 1;
        }

        command = ((uint64_t)pckt_count << 32) | send;
        wiced_rtos_push_to_queue(&comm_queue, &command, 0);

        wiced_watchdog_kick(); //need to kick watchdog to avoid a reset, see documentation for this function
    }

    nano_test_start = wiced_get_nanosecond_clock_value();
    //compute delta if needed

    //wait for other thread to be done, then go to hib.
    wiced_rtos_thread_join(&udp_network_thread);
    send_to_hibernate();


    return;
}
