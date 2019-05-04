
wiced_result_t blink(int which, int freq, wiced_timer_t *timer);
void stop_blink(int red, int green, wiced_timer_t *timer);
void init_leds(int on);
void led_show(int which, int on);
