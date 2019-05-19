#include "../headers/WASP.h"

void spi_startup(void *arg);
void sample_main(void *arg);

void run_calibration(void *arg);

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
    result = wiced_rtos_init_queue(&spi_starup_queue, NULL, 4, 1);
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ( "good test code queue\n"));

    //adc stuff - it exists in a different thread
    //ideally we would want this in the same thread, but it needs a bigger stack
    //and we cant seem to give the main thread a bigger stack through wiced_defaults.h

    wiced_thread_t spi_init_thread;
    result = wiced_rtos_create_thread (&spi_init_thread, 0, "", spi_startup, 10*1024, 0);

    wiced_rtos_thread_join(&spi_init_thread); //wait for it to finish
    uint32_t test_codes;
    result = wiced_rtos_pop_from_queue(&spi_starup_queue, &test_codes, 0);

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
    WPRINT_APP_INFO( ( "batt level = %d\n", batt_level) ); //debug
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
        //maybe send back to hib in this case
        send_to_hibernate();
        return;
    }

    //check whether we got the command to calibrate
    if(server_init_cmds.calibrate == 1)
    {
        WPRINT_APP_INFO( ( "Got calibration command.\n") );
        WPRINT_APP_INFO( ( "Calibration will begin in 30sec.\n") );

        result = blink(1, 500, &timer); //blink the red led until the test starts
        wiced_rtos_delay_milliseconds(30000);
        stop_blink(1, 0, &timer);

        result = blink(3, 500, &timer); //blink both until calibration is done
        WPRINT_APP_INFO( ( "Running calibration test.\n") );
        //calibration involves sampling, need to do another thread

        wiced_thread_t calibration_thread;
        result = wiced_rtos_create_thread (&calibration_thread, 0, "", run_calibration, 10*1024, 0);
        wiced_rtos_thread_join(&calibration_thread); //wait for it to finish

        stop_blink(1, 1, &timer);
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
    WPRINT_APP_INFO( ( "port %d\n", port) );
    platform_init_nanosecond_clock();

    //create udp socket
    init_udp(port);
    WPRINT_APP_INFO( ( "created udp socket at port %d\n", port) );

    //sync with ntp
    WPRINT_APP_INFO( ( "starting time sync..\n") );
    sntp_start_auto_time_sync( TIME_SYNC_PERIOD );
    WPRINT_APP_INFO( ( "finished.\n") );

    //create queues and fill them like normal
    result = init_queues(); //not needed by non perfect function
    WPRINT_APP_INFO( ( "queues ready.\n") );

    //spawn tcp async thread?
    //wiced_thread_t tcp_async_thread; does this need to be a thread??
    //tests say no thread needed
    tcp_server_start_async();

    //wait for the test start broadcast
    WPRINT_APP_INFO( ( "waiting for broadcast\n") );
    result = rx_udp();
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ( "start test\n") );


    //sample and send thread
    wiced_thread_t dummy;
    result = wiced_rtos_create_thread (&dummy, 4, "send_samples", tx_udp_packet_known_good, 10*1024, NULL);
    wiced_rtos_thread_join(&dummy);


    //cant get these to work for the life of me - something is messed up that I cant seem to figure out after weeks
    //result = wiced_rtos_create_thread (&udp_network_thread, 4, "udp", send_wasp_packets, 10*1024, &port);
    //if(result == WICED_SUCCESS)
    //    WPRINT_APP_INFO( ( "udp thread started\n") );
    //else
    //    WPRINT_APP_INFO( ( "udp thread ERROR!\n") );

    //spawn producer thread
    /*
    wiced_thread_t spi_sample_thread;
    result = wiced_rtos_create_thread (&spi_sample_thread, 4, "spi", sample_main, 15*1024, NULL);
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ( "spi thread started\n") );
    else
        WPRINT_APP_INFO( ( "spi thread ERROR!\n") );

    wiced_rtos_thread_join(&spi_sample_thread);
    */

    return;
}

void run_calibration(void *arg)
{
    /*
     * this function (run in its own thread cause it does spi) is responsible
     * for taking 1000 samples and averaging them, as well as sending that average
     * back to the server so it can store that data for post processing.
     */

    uint16_t samples[1000]; //if we want to do more than this we will need to a running average
                            //we dont want to consume too much stack (wed need to give it more)

    //spi variables
    uint16_t cur_sample = 0;
    int16_t nsegments = 2;
    wiced_spi_message_segment_t message[1];
    uint8_t txbuf[2], rxbuf[2];
    txbuf[0] = 0xFF;
    txbuf[1] = 0xFF;
    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;
    message[0].length = 2;

    for(int i = 0; i < 1000; i++)
    {
        wiced_gpio_output_high(WICED_GPIO_34);
        wiced_gpio_output_high(WICED_GPIO_34);
        wiced_gpio_output_low(WICED_GPIO_34);
        if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
        {
            //will need to retry this sample
            i--;
        }
        cur_sample = ((uint16_t)rxbuf[0] << 8) | rxbuf[1];
        samples[i] = cur_sample;
        wiced_rtos_delay_milliseconds(1000); //what should this be
    }

    //got the data, do the average
    float sum = 0;
    float average = 0;

    for(int j = 0; j < 1000; j++)
    {
        sum += samples[j];
    }

    average = sum/1000;

    //send the average to the server
    //we shouldnt have dissconected, so we should just be able to send the data back

    send_calibration_data(average);

    return;
}


void spi_startup(void *arg)
{
    UNUSED_PARAMETER(arg);

    wiced_result_t res;
    uint32_t testcodes = 0;
    uint8_t one = 1;

    adc_adxl_setup(); //init gpios

    res = adc_unset_high_z();
    if(res == WICED_SUCCESS)
    {
        testcodes |= one;
        WPRINT_APP_INFO( ( "adc reset success\n") );
    }

    res = adxl_self_test();
    if(res == WICED_SUCCESS)
    {
        testcodes |= (one<<1);
        WPRINT_APP_INFO( ( "self test pass\n") );
    }

    res = adc_set_high_z();
    if(res == WICED_SUCCESS)
    {
        testcodes |= (one<<2);
        WPRINT_APP_INFO( ( "set high z success\n") );
    }
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
    uint32_t comm_message;
    uint32_t loopcount = 0;
    uint32_t pckt_count = 0;
    uint64_t command;
    uint32_t send = 0;
    wiced_result_t ret;
    wasp_pckt_t raw_data;
    int tx_count = 0;

    //platform_init_nanosecond_clock();

    int16_t nsegments = 2;
    wiced_spi_message_segment_t message[1];
    uint8_t txbuf[2], rxbuf[2];

    //send all ones for  conversion
    txbuf[0] = 0xFF;
    txbuf[1] = 0xFF;
    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;
    message[0].length = 2;
    WPRINT_APP_INFO( ( "begin sampling\n") );

    nano_test_start = wiced_get_nanosecond_clock_value(); //mark it just in case, nut test ends will be async
    while(!test_concluded)
    {
        //this isnt EXACTLY regular - TODO.
        wiced_time_get_iso8601_time( &iso8601_time );
        nano_time = wiced_get_nanosecond_clock_value();
        //wiced_rtos_push_to_queue(&iso_time_queue, &iso8601_time, 0);
        //wiced_rtos_push_to_queue(&nano_time_queue, &nano_time, 0);

        raw_data.packet_count = tx_count++;
        raw_data.time_start = iso8601_time;
        raw_data.nano_time_start = nano_time;

        for(int i = 0; i < 118; i++)
        {
            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_low(WICED_GPIO_34);
            wiced_spi_transfer(&spi_device , message, nsegments);
            sample1 = ((uint16_t)rxbuf[0] << 8) | rxbuf[1];

            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_high(WICED_GPIO_34);
            wiced_gpio_output_low(WICED_GPIO_34);
            wiced_spi_transfer(&spi_device , message, nsegments);
            sample2 = ((uint16_t)rxbuf[0] << 8) | rxbuf[1];


            //comm_message = ((uint32_t)sample1 << 16) | sample2;
            //wiced_rtos_push_to_queue(&main_queue, &comm_message, 0);

            raw_data.samples[i] = sample1;
            raw_data.samples[i+1] = sample2;
        }

        //create message for the other thread
        loopcount++;
        if(loopcount >= 3)
        {
            pckt_count++;
            send = 1;
        }

        //command = ((uint64_t)pckt_count << 32) | send;
        //wiced_rtos_push_to_queue(&comm_queue, &command, 0);

        //wiced_watchdog_kick(); //need to kick watchdog to avoid a reset, see documentation for this function

        //test only, this should be deleted
        tx_udp_packet();
        //tx_udp_packet_arg(&raw_data);
    }

    nano_test_end = wiced_get_nanosecond_clock_value();
    //compute delta if needed

    //wait for other thread to be done, then go to hib.
    //wiced_rtos_thread_join(&udp_network_thread);
    send_to_hibernate();


    return;
}
