#define VREF 3.3 //WASP
#define mV_per_g 2.64 //for 3.3v ref to adxl

#define SELF_TEST_LOW_BOUND 0.015
#define SELF_TEST_HIGH_BOUND 0.03

int over_range;

void over_range_isr(void* arg);
/*
 * note: we determined on our v1 board that when we send the board into hibernate, the pin
 * goes tri-state, so setting standby before going to hibernate wont work.
 * effectively, its unusable until we make a new board where we add hardware to fix the problem.
 * These functions will be left in anyways for future us, but we wont use them.
 */
void set_adxl_standby(void);
void clear_adxl_standby(void);
void adc_adxl_setup(void);
inline uint16_t adc_sample(void);
wiced_result_t adc_set_high_z(void);
wiced_result_t adc_unset_high_z(void);
wiced_result_t adxl_self_test(void);
