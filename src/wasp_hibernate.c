#include "wiced.h"
#include "../headers/wasp_hibernate.h"
#include "../headers/wasp_sflash_store.h"

void send_to_hibernate()
{
    uint32_t          ticks_to_wakeup;
    uint32_t          seconds_to_wakeup;
    uint32_t          freq;
    uint32_t          max_ticks;
    platform_result_t result = PLATFORM_BADARG;

    printf( "Entering hibernate state for %u ms | %d min\n", (unsigned)HIBERNATION_PERIOD_MS, HIBERNATION_PERIOD_MIN);

    freq      = platform_hibernation_get_clock_freq( );
    max_ticks = platform_hibernation_get_max_ticks( );

    //do in 2 steps to avoid overflow
    seconds_to_wakeup = HIBERNATION_PERIOD_MS/1000;
    ticks_to_wakeup = freq*seconds_to_wakeup;
    if ( ticks_to_wakeup > max_ticks )
    {
        printf( "Scheduled ticks number %u is too big\n", (unsigned)ticks_to_wakeup );
    }
    else
    {
        result = platform_hibernation_start( ticks_to_wakeup );
    }

    while ( 1 )
    {
        printf( "Hibernation failure: %d\n", (int)result );
        host_rtos_delay_milliseconds( 1000 );
        result = platform_hibernation_start( ticks_to_wakeup ); //just in case?
    }
}

int is_hib_wake(void)
{
    uint32_t          freq;

    freq      = platform_hibernation_get_clock_freq( );

    if ( platform_hibernation_is_returned_from( ) )
    {
        uint32_t spent_ticks = platform_hibernation_get_ticks_spent( );
        //will get overflow if divide isnt done first.
        uint32_t spent_ms    = spent_ticks / freq;
        spent_ms = spent_ms*1000;
        float spent_min   = spent_ms / 60000.0;

        printf("Returned from hibernation: spent there %u ticks, %u ms, %f min\n\n", (unsigned)spent_ticks, (unsigned)spent_ms, spent_min);
        return 1; //return 1 if we returned from hib
    }
    else
        return 0;
}

void send_to_OTA(char* host_string)
{
    uint32_t          ticks_to_wakeup;
    uint32_t          seconds_to_wakeup;
    uint32_t          freq;
    uint32_t          max_ticks;
    platform_result_t result = PLATFORM_BADARG;

    printf("Rebooting to do update.\n");

    freq      = platform_hibernation_get_clock_freq( );
    max_ticks = platform_hibernation_get_max_ticks( );

    set_ota_boot(host_string);

    //just want to reboot - send to sleep for 1 second
    seconds_to_wakeup = 1;
    ticks_to_wakeup = freq*seconds_to_wakeup;
    if ( ticks_to_wakeup > max_ticks )
    {
        printf( "Scheduled ticks number %u is too big\n", (unsigned)ticks_to_wakeup );
    }
    else
    {
        result = platform_hibernation_start( ticks_to_wakeup );
    }

    while ( 1 )
    {
        printf( "Hibernation failure: %d\n", (int)result );
        host_rtos_delay_milliseconds( 1000 );
        result = platform_hibernation_start( ticks_to_wakeup ); //just in case?
    }
}
