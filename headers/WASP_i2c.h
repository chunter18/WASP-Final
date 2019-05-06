/*
 * reg map
 * addr     name    desc            readonly    default
 * 0x00     A       status              y       0x80
 * 0x01     B       control             N       0x7F
 * 0x02     C       accumchrg msb       N       0xFF
 * 0x03     D       accumchrg lsb       N       0xFF
 * 0x04     E       chrgthrsh hi msb    N       0xFF
 * 0x05     F       chrgthrsh hi lsb    N       0xFF
 * 0x06     G       chrgthrsh lo msb    N       0x00
 * 0x07     G       chrgthrsh lo lsb    N       0x00
 */

/*
 * Status reg A bit enumeration
 * Bit     name         operation               default
 * A[7]    Chip ID      1=LTC2491                   1
 * A[6]     not used
 * A[5]     accumulatedd charge over/underflow      0
 * a[4]     not used
 * a[3]     charge alert hight                      0
 * a[2]     charge alert low                    `   0
 * a[1]     vbat alert                              0
 * a[0]     undervoltage lockout alert               X
 */

/*
 * control reg B bit enumeration
 * Bit      Name            op              default
 * B[7:6]   Vbat alert      11=3v thresh    00
 *                          10=2.9v thresh
 *                          01=2.8v thresh
 *                          00=vbat off
 *B[5:3]    prescaler M     eq              111 (128)
 *                   M=2^(4*B[5] + 2*B[4] + B[3])
 *B[2:1]    AL/CC config    10=alert mode   10
 *B[2:1]                    01=charge complete mode
 *                          00=pin disabled
 *                          11=not allowed
 *b[0]      shutdown        shutdown analog    0
 *                          section (1=shutdown)
 */



#define LTC2941_SLAVE_ADDR         (0x64)
#define LTC2941_SLAVE_ADDR_BINARY  (0b1100100) //7 bit addr
#define LTC2941_CONFIG_BASE        (0b11000100)

//these are the internal resistor values
#define RESISTOR_VAL_OHMS           0.05
#define RESISTOR_VAL_MILLIOHMS      50

#define NUM_I2C_MESSAGE_RETRIES     (3)

//#define BATTERY_CAPACITY 100  //mah
//#define BATTERY_CAPACITY 500  //mah
//#define BATTERY_CAPACITY 1000 //mah
//#define BATTERY_CAPACITY 1200 //mah
//#define BATTERY_CAPACITY 1500 //mah
//#define BATTERY_CAPACITY 2000 //mah
#define BATTERY_CAPACITY 2500 //mah

#define LTC2941_PRESCALAR_M_1                   0x00
#define LTC2941_PRESCALAR_M_2                   0x08
#define LTC2941_PRESCALAR_M_4                   0x10
#define LTC2941_PRESCALAR_M_8                   0x18
#define LTC2941_PRESCALAR_M_16                  0x20
#define LTC2941_PRESCALAR_M_32                  0x28
#define LTC2941_PRESCALAR_M_64                  0x30
#define LTC2941_PRESCALAR_M_128                 0x38


wiced_result_t LTC2941_init_startup();
wiced_result_t LTC2941_init();
uint8_t LTC2941_1reg_read(uint8_t regaddr);
wiced_result_t LTC2941_1reg_write(uint8_t regaddr, uint8_t write_byte);
uint16_t LTC2941_2reg_read(uint8_t first_regaddr);
wiced_result_t LTC2941_2reg_write(uint8_t first_regaddr, uint16_t write_short);
float LTC2941_get_coulombs(uint16_t prescalar);
float LTC2941_get_mAh(uint16_t prescalar );
float LTC2942_get_voltage();
uint8_t find_prescalar();
uint8_t battery_percentage(uint16_t prescalar);
