/*
example usage
wiced_timer_t timer;
res = blink(3, 500, &timer); //freq is in ms
WPRINT_APP_INFO( ("result = %d",res));
WPRINT_APP_INFO( ("delay 10s"));
res = wiced_rtos_delay_milliseconds(10000);
stop_blink(1, 0, &timer);
WPRINT_APP_INFO( ("done"));
*/

#include "wiced.h"
#include "../headers/WASP_LED.h"

static void _blink(int which) //not working, need to add printing
{
    wiced_bool_t greenon;
    wiced_bool_t redon;
    int w = which;
    //WPRINT_APP_INFO( ("w=%d\n", w));
    switch(w)
    {
        case 1: //red
            redon = wiced_gpio_input_get(WICED_GPIO_29);
            if(redon)
                wiced_gpio_output_low( WICED_GPIO_29 );
            else
                wiced_gpio_output_high( WICED_GPIO_29 );
            break;
        case 2: //green
            greenon = wiced_gpio_input_get(WICED_GPIO_28);
            if(greenon)
                wiced_gpio_output_low( WICED_GPIO_28 );
            else
                wiced_gpio_output_high( WICED_GPIO_28 );
            break;
        case 3:
            greenon = wiced_gpio_input_get(WICED_GPIO_28);
            redon = wiced_gpio_input_get(WICED_GPIO_29);
            if(redon)
                wiced_gpio_output_low( WICED_GPIO_29 );
            else
                wiced_gpio_output_high( WICED_GPIO_29 );
            if(greenon)
                wiced_gpio_output_low( WICED_GPIO_28 );
            else
                wiced_gpio_output_high( WICED_GPIO_28 );
            break;
        default:
            wiced_gpio_output_high( WICED_GPIO_28 );
            wiced_gpio_output_high( WICED_GPIO_29 );
    }
}

wiced_result_t blink(int which, int freq, wiced_timer_t *timer) //freq is in ms
{
    wiced_result_t result;

    result = wiced_rtos_init_timer(timer, freq, _blink, which);
    wiced_rtos_start_timer(timer);
    return result;
}

void stop_blink(int red, int green, wiced_timer_t *timer)
{
    wiced_rtos_stop_timer(timer);
    if(red)
        wiced_gpio_output_high( WICED_GPIO_29 ); //red
    else
        wiced_gpio_output_low( WICED_GPIO_29 ); //red

    if(green)
        wiced_gpio_output_high( WICED_GPIO_28 ); //green
    else
        wiced_gpio_output_low( WICED_GPIO_28 ); //green
}

void init_leds(int on)
{
    wiced_gpio_init(WICED_GPIO_28, OUTPUT_PUSH_PULL);
    wiced_gpio_init(WICED_GPIO_29, OUTPUT_PUSH_PULL);

    if(on)
    {
        wiced_gpio_output_high( WICED_GPIO_28 ); //green
        wiced_gpio_output_high( WICED_GPIO_29 ); //red
    }
    else
    {
        wiced_gpio_output_low( WICED_GPIO_28 ); //green
        wiced_gpio_output_low( WICED_GPIO_29 ); //red
    }
}

void led_show(int which, int on)
{
    switch(which)
    {
        case 1: //red
            if(on)
                wiced_gpio_output_high( WICED_GPIO_29 );
            else
                wiced_gpio_output_low( WICED_GPIO_29 );
            break;
        case 2: //green
            if(on)
                wiced_gpio_output_high( WICED_GPIO_28 );
            else
                wiced_gpio_output_low( WICED_GPIO_28 );
            break;
        case 3:
            if(on)
            {
                wiced_gpio_output_high( WICED_GPIO_29 );
                wiced_gpio_output_high( WICED_GPIO_28 );
            }
            else
            {
                wiced_gpio_output_low( WICED_GPIO_29 );
                wiced_gpio_output_low( WICED_GPIO_28 );
            }
            break;
    }
}
