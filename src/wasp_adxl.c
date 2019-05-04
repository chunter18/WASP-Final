#include "wiced.h"
#include "wiced_osl.h"
#include "../headers/wasp_adxl.h"

wiced_spi_device_t spi_device =
{
    .port               = WICED_SPI_1,
    .chip_select        = WICED_GPIO_22, //CS PIN IN 43097
    .speed              = 1000000,       //1mhz base for bb, if #define is ther can change it here
    .mode               = ( SPI_CLOCK_RISING_EDGE | SPI_CLOCK_IDLE_LOW | SPI_MSB_FIRST ),
    .bits               = 8
};

int over_range = 0; //over range flag

void over_range_isr(void* arg)
{
    UNUSED_PARAMETER(arg);
    //WPRINT_APP_INFO(("OVER RANGE DETECTED!!\n"));
    over_range = 1;
}

void adc_adxl_setup(void)
{
    //initialize the spi device
    if( wiced_spi_init(&spi_device) != WICED_SUCCESS)
        WPRINT_APP_INFO(( "SPI device init fail\n" ));
    else
        WPRINT_APP_INFO(( "SPI device init success\n" ));

    wiced_gpio_init(WICED_GPIO_34, OUTPUT_PUSH_PULL); //cnv pin
    wiced_gpio_init(WICED_GPIO_17, OUTPUT_PUSH_PULL); //ping - adxl standby
    wiced_gpio_init(WICED_GPIO_21, OUTPUT_PUSH_PULL); //sdi pin, need to idle high
    wiced_gpio_init(WICED_GPIO_12, OUTPUT_PUSH_PULL);  //ADXL_test is connected to GPIO_16 which is WICED_GPIO_12 - pinr quicksilver
    //wiced_gpio_init(WICED_GPIO_6, INPUT_HIGH_IMPEDANCE); //pin h, over range pin

    //for timing
    wiced_gpio_init(WICED_GPIO_35, OUTPUT_PUSH_PULL); //pin m, the test pin we assigned
    wiced_gpio_init(WICED_GPIO_36, OUTPUT_PUSH_PULL); //pinp in case we fucked it
    wiced_gpio_output_low(WICED_GPIO_35); //no need for these, but ill leave them for timing use for someone else
    wiced_gpio_output_high(WICED_GPIO_36);

    wiced_gpio_output_low(WICED_GPIO_34); //init cnv to low, pulse high to take sample.
    wiced_gpio_output_high(WICED_GPIO_21); //sdi idle high
    wiced_gpio_output_low(WICED_GPIO_17); //dont be in standby
    wiced_gpio_output_low(WICED_GPIO_12); //initialize to not in self test

    //init the over range isr
    wiced_gpio_input_irq_enable(WICED_GPIO_6, IRQ_TRIGGER_LEVEL_HIGH, over_range_isr, NULL); /* Setup interrupt */
    //gpio is normally low, goes high when overrange detected, goes periodic every 500us if sustained


    return;
}

uint16_t adc_sample(void)
{
    //might be good to include picture of what we want in the github - TODO

    //spi transfer variables
    //these could be global - we should do the timing test.
    int16_t nsegments = 2;
    wiced_spi_message_segment_t message[1];
    uint8_t txbuf[2], rxbuf[2];

    //send all ones for  conversion
    txbuf[0] = 0xFF;
    txbuf[1] = 0xFF;
    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;
    message[0].length = 2;

    //send the conversion pulse. (short time high, back to low)
    //we write high twice to ensure that the high time is long enought to trigger the conversion.
    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_low(WICED_GPIO_34);
    if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
    {
        return -1;
        //WPRINT_APP_INFO(("SPI slave send failed at master side\n"));
    }

    /*
     * This was from the ADI engineer - he said there should be a pulse on the other side of a sample.
     * from the knowlege we gained from the eval kit, we know that the back side pulse is irrelevant.
    else
    {
        PLATFORM_CHIPCOMMON->gpio.output |= (   1 << 31 ); //high
        PLATFORM_CHIPCOMMON->gpio.output |= (   1 << 31 ); //high
        PLATFORM_CHIPCOMMON->gpio.output &= (~( 1 << 31 )); //low
        //WPRINT_APP_INFO(("SPI slave send success\n"));
    }
    */

    uint16_t sample = ((uint16_t)rxbuf[0] << 8) | rxbuf[1];
    return sample;
}

void set_adxl_standby(void)
{
    wiced_gpio_output_high(WICED_GPIO_17); //ping - sets standby
    return;
}
void clear_adxl_standby(void)
{
    wiced_gpio_output_low(WICED_GPIO_17); //ping - make sure not in standby
    return;
}

wiced_result_t adc_set_high_z(void)
{
    //transfer variables
    int16_t nsegments = 2;
    wiced_spi_message_segment_t message[1];
    uint8_t txbuf[2] , rxbuf[2];

    txbuf[0] = 0x14; //write command
    txbuf[1] = 0xE5; //write high z with mask
    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;
    message[0].length = 2;

    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_low(WICED_GPIO_34);
    if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
    {
       //WPRINT_APP_INFO(("SPI slave send failed at master side\n"));
        return WICED_ERROR;
    }
    /*
    else
    {
       PLATFORM_CHIPCOMMON->gpio.output |= (   1 << 31 ); //pull back to high
       //WPRINT_APP_INFO(("SPI slave send success\n"));
    }
    */

    //next we want to read the data back to check it was successful
    //we need it to be correct or else not all our data will be correct.
    txbuf[0] = 0x54; //read reg command
    txbuf[1] = 0x00; //send all zeros so it doesnt get confused.

    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;

    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_low(WICED_GPIO_34);
    if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
    {
        //WPRINT_APP_INFO(("SPI slave send failed at master side\n"));
        return WICED_ERROR;
    }

    /*
    else
    {
        PLATFORM_CHIPCOMMON->gpio.output |= (   1 << 31 ); //high
    }
    */

    //check the output. all we care about is the second byte, first will always be 0x80
    if(rxbuf[1] == 0x65) //high z enabled!
        return WICED_SUCCESS;
    else
        return WICED_ERROR;
}

wiced_result_t adc_unset_high_z(void)
{
    //transfer variables
    int16_t nsegments = 2;
    wiced_spi_message_segment_t message[1];
    uint8_t txbuf[2] , rxbuf[2];

    txbuf[0] = 0x14; //write command
    txbuf[1] = 0xE1; //write high z with mask
    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;
    message[0].length = 2;

    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_low(WICED_GPIO_34);
    if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
    {
       //WPRINT_APP_INFO(("SPI slave send failed at master side\n"));
        return WICED_ERROR;
    }

    /*
    else
    {
       PLATFORM_CHIPCOMMON->gpio.output |= (   1 << 31 ); //pull back to high
       //WPRINT_APP_INFO(("SPI slave send success\n"));
    }
    */

    //next we want to read the data back to check it was successful
    //we need it to be correct or else not all our data will be correct.
    txbuf[0] = 0x54; //read reg command
    txbuf[1] = 0x00; //send all zeros so it doesnt get confused.

    message[0].tx_buffer = txbuf;
    message[0].rx_buffer = rxbuf;

    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_high(WICED_GPIO_34);
    wiced_gpio_output_low(WICED_GPIO_34);
    if (wiced_spi_transfer(&spi_device , message, nsegments)!=WICED_SUCCESS)
    {
        //WPRINT_APP_INFO(("SPI slave send failed at master side\n"));
        return WICED_ERROR;
    }
    /*
    else
    {
        PLATFORM_CHIPCOMMON->gpio.output |= (   1 << 31 ); //high
    }
    */

    //check the output. all we care about is the second byte, first will always be 0x80
    if(rxbuf[1] == 0x61)
        return WICED_SUCCESS;
    else
        return WICED_ERROR;
}

wiced_result_t adxl_self_test(void)
{
    uint16_t sample1 = 0;
    uint16_t sample2 = 0;
    uint16_t delta = 0;
    double step;
    double scaled_v;
    double V;

    sample1 = adc_sample(); //measure voltage without self test pin on
    if(sample1 == -1)
        return WICED_ERROR;

    wiced_gpio_output_high(WICED_GPIO_12); //turn on self test pin
    wiced_rtos_delay_microseconds(1000); //wait for the internal operation to be ready

    sample2 = adc_sample(); //measure output again
    if(sample1 == -1)
        return WICED_ERROR;

    //do math so output is always positive. we only care about the diff
    if(sample1 >= sample2)
        delta = sample1-sample2;
    else
        delta = sample2-sample1;

    //compute the voltage for the delta using max accuracy
    step = 3300000.0/65536.0;
    scaled_v = step*delta;
    V = scaled_v /1000000;

    //print, if needed
    //WPRINT_APP_INFO( ("\nSTEP = %Lf, sample = %d, Voltage = %Lf\n", step, delta, V));

    //determine pass or fail
    if((V >= SELF_TEST_HIGH_BOUND) || (V <= SELF_TEST_LOW_BOUND)) //22mv on the graph, daniel says pass if between 15 and 30
    {
        //self test fail, return
        wiced_gpio_output_low(WICED_GPIO_12);
        wiced_rtos_delay_microseconds(300);
        return WICED_ERROR;
    }
    else
    {
        //self test pass
        wiced_gpio_output_low(WICED_GPIO_12); //release the pin
        wiced_rtos_delay_microseconds(300); //results will be valid again after this time
        return WICED_SUCCESS;
    }
}
