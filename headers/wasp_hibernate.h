
//#define HIBERNATION_PERIOD_MS 10000  //10 seconds
//#define HIBERNATION_PERIOD_MS 60000  //1 min
//#define HIBERNATION_PERIOD_MS 300000 //5 min
#define HIBERNATION_PERIOD_MS 600000 //10 mins

#define HIBERNATION_PERIOD_MIN HIBERNATION_PERIOD_MS/60000

#define PLATFORM_HIB_ENABLE

void send_to_hibernate(void);
void send_to_OTA(char* host_string);
int is_hib_wake(void);
