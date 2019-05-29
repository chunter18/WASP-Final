

extern wiced_queue_t main_queue;
extern wiced_queue_t iso_time_queue;
extern wiced_queue_t nano_time_queue;
extern wiced_queue_t comm_queue;

wiced_result_t init_queues();
wiced_result_t pre_fill_queues();
wiced_bool_t nano_delta(uint64_t timestart);
