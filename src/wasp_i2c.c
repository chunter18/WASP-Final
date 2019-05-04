#include "wiced.h"
#include "../headers/WASP_i2c.h"
#include <math.h>

static wiced_i2c_device_t i2c_device_gas_gauge =
{
        .port = WICED_I2C_1, //we are using i2c_0 for wasp, denoted as I2C_1 in the makefiles
        .address = LTC2941_SLAVE_ADDR_BINARY,   //hardcoded slave address
        .address_width = I2C_ADDRESS_WIDTH_7BIT,
        .speed_mode = I2C_STANDARD_SPEED_MODE, //ltc2491 supports 100 and 400khz
                                               //we will be using 100 because its plenty fast
};

//these are ltc2942 values - do they need to be changed?
const float LTC2941_CHARGE_lsb = 0.085E-3;
const float LTC2941_VOLTAGE_lsb = .3662E-3;
const float LTC2941_FULLSCALE_VOLTAGE = 6;

wiced_result_t LTC2941_init()
{
    //intiializing the gas guage part. To do this we first need to initilize the i2c interface.
    /* Initialize I2C */
    WPRINT_APP_INFO( ( "I2C Initialization\n" ) );
    if ( wiced_i2c_init( &i2c_device_gas_gauge ) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ( "I2C Initialization Failed\n" ) );
        return WICED_ERROR;
    }

    //with the i2c bus up, we can now init the actual device
    //we do so by reading the status reg at add 0x00 and checking to see if it aligns
    //with what we are expecting.

    uint8_t reg_content;
    reg_content = LTC2941_1reg_read(0x00);
    //mask the byte read so we are only looking at bit 7
    reg_content = (reg_content&0x80); //reg content AND 1000_0000
    if(reg_content == 0x80)
    {
        WPRINT_APP_INFO( ( "LTC2491 connected, initialization succeeded. \n" ) );
        return WICED_SUCCESS;
    }
    else
    {
        WPRINT_APP_INFO( ( "Status reg returned bad arg: 0x%x\n",reg_content ) );
        return WICED_ERROR;
    }
}

wiced_result_t LTC2941_init_startup()
{
    uint8_t ltc2941_config_byte = 0;
    wiced_result_t result;

    result = LTC2941_init();
    if(result != WICED_SUCCESS)
        return result;

    WPRINT_APP_INFO( ( "First time startup, bringing up AL/CC. \n" ) );

    //next we need to set up the vbat alert.
    //we need to write to the control reg that alert is on, and the level is 3v
    //binary format of that write is 11_XXX_10_0 - stored in LTC2941_config_byte
    //xxx is the prescaler bits

    uint8_t m = find_prescalar();
    switch(m)
    {
        case (1):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_1|LTC2941_CONFIG_BASE;
            break;
        case (2):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_2|LTC2941_CONFIG_BASE;
            break;
        case (4):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_4|LTC2941_CONFIG_BASE;
            break;
        case (8):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_8|LTC2941_CONFIG_BASE;
            break;
        case (16):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_16|LTC2941_CONFIG_BASE;
            break;
        case (32):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_32|LTC2941_CONFIG_BASE;
            break;
        case (64):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_64|LTC2941_CONFIG_BASE;
            break;
        case (128):
            ltc2941_config_byte = LTC2941_PRESCALAR_M_128|LTC2941_CONFIG_BASE;
            break;
    }

    WPRINT_APP_INFO( ( "config byte =  0x%x\n",ltc2941_config_byte ) );

    if ( LTC2941_1reg_write(0x01, ltc2941_config_byte) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ( "failed to bring up AL/CC\n" ) );
        return WICED_ERROR;
    }

    //getting adc_code to write to the registers
    //float qlsb = LTC2941_CHARGE_lsb*(m/128); //ide says this is unused
    float regval;
    //regval = BATTERY_CAPACITY/qlsb;

    uint16_t write_code;
    uint8_t msb, lsb;
    regval = BATTERY_CAPACITY*(RESISTOR_VAL_OHMS*128)/(LTC2941_CHARGE_lsb*m*50E-3*1000.0);
    //mAh_charge = 1000*(float)(adc_code*LTC2941_CHARGE_lsb*prescalar*50E-3)/(RESISTOR_VAL_OHMS*128);

    regval = roundf(regval);
    if(regval > 65535)
        write_code = 0xFF;
    else
        write_code = (uint16_t)regval;
    WPRINT_APP_INFO(("regval = %lf, writing %d\n",regval, write_code));

    msb = (uint8_t)((write_code & 0xFF00) >> 8);
    lsb = (uint8_t)(write_code & 0x00FF);
    WPRINT_APP_INFO(( "16->0x%x msb->0x%x, lsb->0x%x\n",write_code, msb, lsb));

    result = LTC2941_2reg_write(0x02, regval);
    if(result != WICED_SUCCESS)
        return result;

    return WICED_SUCCESS;

}

uint8_t LTC2941_1reg_read(uint8_t regaddr)
{
    wiced_result_t result;

    uint8_t data = 0; //read contents go here

    //send the address of the reg we want to read
    result = wiced_i2c_write( &i2c_device_gas_gauge, WICED_I2C_START_FLAG, &regaddr, 1 );
    //read the reg contents from the device
    result = wiced_i2c_read( &i2c_device_gas_gauge,WICED_I2C_REPEATED_START_FLAG | WICED_I2C_STOP_FLAG, &data, 1 );

    if(result == WICED_SUCCESS)
    {
        return data;
    }
    else
    {
        WPRINT_APP_ERROR( ( "I2C 1 reg read failed\n"));
        return -1;
    }
}

uint16_t LTC2941_2reg_read(uint8_t first_regaddr)
{
    //wiced_result_t result;

    uint8_t data[2] = {0,0}; //read contents go here
    uint16_t sample = 0;

    //this was doing bad stuff, going to try to go around it
    //send the address of the reg we want to read
    //result = wiced_i2c_write( &i2c_device_gas_gauge, WICED_I2C_START_FLAG, &first_regaddr, 2 );
    //read the reg contents from the device
    //result = wiced_i2c_read( &i2c_device_gas_gauge,WICED_I2C_REPEATED_START_FLAG | WICED_I2C_STOP_FLAG, &data, 2 );

    data[0] = LTC2941_1reg_read(first_regaddr);
    data[1] = LTC2941_1reg_read( (first_regaddr+1) );

    sample = ((uint16_t)data[0] << 8) | data[1];
    return sample;

}

wiced_result_t LTC2941_1reg_write(uint8_t regaddr, uint8_t write_byte)
{
    uint8_t cmd[2] = {regaddr, write_byte};

    //first, write the actual sequence we want to write (reg address, byte to go into that reg).
    wiced_i2c_write( &i2c_device_gas_gauge, WICED_I2C_START_FLAG | WICED_I2C_STOP_FLAG, &cmd, 2 );

    //next we want to read from that very same reg to confirm it was written correctly.
    uint8_t data = 0;
    data = LTC2941_1reg_read(regaddr);
    if(data == write_byte)
        return WICED_SUCCESS;
    else
        return WICED_ERROR;
}

wiced_result_t LTC2941_2reg_write(uint8_t first_regaddr, uint16_t write_short)
{
    uint8_t msb = (uint8_t)((write_short & 0xFF00) >> 8);
    uint8_t lsb = (uint8_t)(write_short & 0x00FF);
    uint8_t cmd[3] = {first_regaddr, msb, lsb};

    wiced_i2c_write( &i2c_device_gas_gauge, WICED_I2C_START_FLAG | WICED_I2C_STOP_FLAG, &cmd, 3 );

    uint16_t rec = LTC2941_2reg_read(first_regaddr);
    //uint8_t h = LTC2941_1reg_read(0x02);
    //uint8_t l = LTC2941_1reg_read(0x03);
    //uint16_t rec = ((uint16_t)h << 8) | l;

    if(rec == write_short)
        return WICED_SUCCESS;
    else
        return WICED_ERROR;
}



float LTC2941_get_coulombs(uint16_t prescalar)
{
    uint16_t adc_code = 0;
    adc_code = LTC2941_2reg_read(0x02);
    float coulomb_charge;
    float mah;
    float qlsb = (LTC2941_CHARGE_lsb*50E-3*prescalar)/(RESISTOR_VAL_OHMS*128); //qlsb from datasheet
    mah =  1000*(float)adc_code*qlsb; //colombs - need to be scaled back to mah from amp hours
    coulomb_charge = mah*3.6f; //1mah = 3.6 coulombs
    return(coulomb_charge);
}

float LTC2941_get_mAh(uint16_t prescalar )
{
  uint16_t adc_code = 0;
  adc_code = LTC2941_2reg_read(0x02);
  float mAh_charge;
  //see get_colombs for a better explanation
  WPRINT_APP_ERROR( ( "%d\n",adc_code));
  mAh_charge = 1000*(float)(adc_code*LTC2941_CHARGE_lsb*prescalar*50E-3)/(RESISTOR_VAL_OHMS*128);
  return(mAh_charge);
}

float LTC2942_get_voltage() //dont use
{
  uint16_t adc_code = 0;
  adc_code = LTC2941_2reg_read(0x02);
  float voltage;
  voltage = ((float)adc_code/(65535))*LTC2941_FULLSCALE_VOLTAGE;
  return(voltage);
}

uint8_t find_prescalar()
{
    int valid_m[8]       = {1,2,4,8,16,32,64,128};
    float differences[8] = {0,0,0,0,0, 0, 0, 0};

    float numerator = 128*(BATTERY_CAPACITY)*RESISTOR_VAL_OHMS;
    float denominator = 65536*(0.085)*50E-3;
    float result = numerator/denominator;

    //WPRINT_APP_INFO(( "m result calculated = %f\n", result));

    for(int i = 0; i < 8; i++)
    {
        differences[i] = result - valid_m[i];
    }

    int index = 0;
    for (int i = 0; i < 8; i++)
    {
        if (differences[i] < 0)
        {
            index = i;
            break;
        }
    }

    uint8_t m = valid_m[index];
    //WPRINT_APP_INFO(( "m chosen = %d\n", m));
    return m;
}

uint8_t battery_percentage(uint16_t prescalar)
{
    float mah = LTC2941_get_mAh(prescalar);
    return (uint8_t)(mah/BATTERY_CAPACITY);
}
