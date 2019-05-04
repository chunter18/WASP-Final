#include "../headers/WASP.h"

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
        WPRINT_APP_INFO( ( "OTA\n") ); //change this to OTA boot function when ready

    //no else needed, if not ota just keep on as per usual

    uint16_t prescaler = find_prescalar();
    wiced_result_t result;
    wiced_timer_t timer;
    uint8_t batt_level;

    int returned_from_hib = is_hib_wake();
    if(returned_from_hib) //we DID just return from hibernate, which means dont set
    {
        wiced_rtos_delay_milliseconds(100); //do we need to wait for v ails here too?
        //check batt level
        batt_level = battery_percentage(prescaler);
    }
    else //first time boot, bring up gas guage
    {
        //delay to let the voltage rails come up
        wiced_rtos_delay_milliseconds(100);
        result = LTC2941_init_startup();
        if(result != WICED_SUCCESS)
        {
            //blink red only - gas gauage didnt come up correctly.
            result = blink(3, 500, &timer); //blink every half second
        }
        batt_level = battery_percentage(prescaler);
    }

    //gas guage is finished, do rest of intialization
    wiced_init(); //initialized the RTOS

    //adc work goes here.

    //bring up the network using last know config
    wiced_network_up(WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL);

    //create the TCP client to register with the server
    result = tcp_init();
    if(result != WICED_SUCCESS)
        WPRINT_APP_INFO( ( "TCP FAILED\n") );

    init_response_t server_init_cmds;
    init_packet_t registration_pckt;
    wiced_mac_t hostmac;

    //fill the registration packet apporpriately.
    hostmac = get_mac();
    memcpy(registration_pckt.mac, hostmac.octet, sizeof(registration_pckt.mac));
    registration_pckt.battery_level = batt_level;
    registration_pckt.test_codes = 0; //fill with adc self test stuff!
    registration_pckt.version_major = 1;
    registration_pckt.version_minor = 0;

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
        //should we disconnnect from tcp? we wont yet but maybe we should.

        //ota boot should still be zero - leave it be.

        //go back to hibernate for another 10 mins.
        send_to_hibernate();
    }
    return;

    /*
     * platform_init_nanosecond_clock();
     * wiced_time_get_iso8601_time( &iso8601_time );
     */
}
