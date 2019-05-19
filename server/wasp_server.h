#include <inttypes.h> //can declare sized ints in there
//for being able to delcare error_t for the arg parsing stuff
#include <stdio.h>
#include <argp.h>


typedef struct //not sure if this is somewhere. here for ease of use/def 
{
	char year[4];        /**< Year         */
	char dash1;          /**< Dash1        */
	char month[2];       /**< Month        */
	char dash2;          /**< Dash2        */
	char day[2];         /**< Day          */
	char T;              /**< T            */
	char hour[2];        /**< Hour         */
	char colon1;         /**< Colon1       */
	char minute[2];      /**< Minute       */
	char colon2;         /**< Colon2       */
	char second[2];      /**< Second       */
	char decimal;        /**< Decimal      */
	char sub_second[6];  /**< Sub-second   */
	char Z;              /**< UTC timezone */
}iso8601_time_t;


typedef struct
{
	uint32_t packet_count;
	iso8601_time_t time_start;
	uint64_t nano_time_start;
	uint16_t samples[236];
}WASP_packet_t; //511 bytes

typedef struct
{
	uint8_t mac[6];
	uint8_t battery_level;
	uint16_t test_codes;
	uint8_t version_major;
	uint8_t version_minor;
}init_packet_t; //initial registration message sent as first message to server

typedef struct
{
	uint8_t fetch_update_command;
	uint8_t send_to_hibernate;
	uint8_t switch_network;
	uint8_t test_begin;
	uint8_t calibrate;
	uint8_t wireless; //placeholder
	uint16_t port; //for ease just going to send the actual port number
}init_response_t; //response message to client device

#define WASP_TCP_SERVER_PORT 50005
#define WASP_TCP_ASYNC_PORT 60005


int check_update(int major, int minor);
int is_testseq_init(void);
int check_switch_net(void);
int check_need_selftest(uint16_t testcode);
int set_wireless_modes(void);
uint16_t assign_port(void);
void *print_data(void *arg);
void start_test(void);
void *wasp_recieve(void *arg);
void *tcp_async_command(void *arg);
static error_t parse_opt (int key, char *arg, struct argp_state *state);
void disqualify(void);
