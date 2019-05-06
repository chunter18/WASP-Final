#include "wiced.h"
#include <inttypes.h>
#include "wiced_osl.h"
#include "../headers/wasp_threads.h"

wiced_queue_t main_queue;
wiced_queue_t iso_time_queue;
wiced_queue_t nano_time_queue;
wiced_queue_t comm_queue;

wiced_result_t init_queues()
{
    wiced_result_t result;
    result = wiced_rtos_init_queue(&main_queue, NULL, 4, 5*118);
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ("Successfully allocated main queue\n") );
    else
    {
        WPRINT_APP_INFO( ("main queue alloc error!\n") );
        return result;
    }

    //8 cause 1 double
    result = wiced_rtos_init_queue(&nano_time_queue, NULL, 8, 5);
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ("Successfully allocated iso time queue\n") );
    else
    {
        WPRINT_APP_INFO( ("iso time queue alloc error!\n") );
        return result;
    }

    //have to allocate an even multiple of 4 - 1 waste byte per entry (no big deal)
    result = wiced_rtos_init_queue(&iso_time_queue, NULL, 28, 5);
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ("Successfully allocated nano time queue\n") );
    else
    {
        WPRINT_APP_INFO( ("nano time queue alloc error!\n") );
        return result;
    }

    //easy inter thread lock.
    //property - retains state kinda
    result = wiced_rtos_init_queue(&comm_queue, NULL, 8, 1);
    if(result == WICED_SUCCESS)
        WPRINT_APP_INFO( ("Successfully allocated communication queue\n") );
    else
    {
        WPRINT_APP_INFO( ("Communication queue alloc error!\n") );
        return result;
    }

    result = pre_fill_queues();
    return result;
}


wiced_result_t pre_fill_queues()
{
    //we observed that pushes to queues go faster when the queues are about half full.
    //here we are pre filling them with garbage data to make pushes of real samples faster
    //which lets us get a better sampling rate.

    wiced_result_t result;
    uint32_t data = rand();

    for(int i= 0; i < 354; i++) //filling main data queue
    {
        result = wiced_rtos_push_to_queue(&main_queue, &data, 0);
        if(result != WICED_SUCCESS)
        {
            //try again
            result = wiced_rtos_push_to_queue(&main_queue, &data, 0);
            if(result != WICED_SUCCESS)
                return result;
        }
    }

    double y = (double)rand();
    wiced_iso8601_time_t z;
    wiced_time_get_iso8601_time( &z );

    //all the other queues have lengths of 5 - so to fill the same amount we just need 3 iterations
    for(int j = 0; j < 3; j++)
    {
        result = wiced_rtos_push_to_queue(&nano_time_queue, &y, 0);
        if(result != WICED_SUCCESS)
        {
            //try again
            result = wiced_rtos_push_to_queue(&nano_time_queue, &y, 0);
            if(result != WICED_SUCCESS)
                return result;
        }

        result = wiced_rtos_push_to_queue(&iso_time_queue, &z, 0);
        if(result != WICED_SUCCESS)
        {
            //try again
            result = wiced_rtos_push_to_queue(&iso_time_queue, &z, 0);
            if(result != WICED_SUCCESS)
                return result;
        }
    }

    //finally, we want to push to the communication queue
    //the data in here lets the send thread know what to do

    uint32_t a = 0; //packet count
    uint32_t b = 0; //send or no
    uint64_t c = ((uint64_t)a << 32) | b;
    result = wiced_rtos_push_to_queue(&comm_queue, &c, 0);

    return result;
}

wiced_bool_t nano_delta(uint64_t timestart)
{
    //uint64_t min = 60000000000;
    uint64_t hour = 3600000000000;
    uint64_t timenow = wiced_get_nanosecond_clock_value();
    //if(timestart+min >= timenow)
    //    return WICED_FALSE;
    if(timestart+hour >= timenow)
        return WICED_FALSE;
    else
        return WICED_TRUE;
}


