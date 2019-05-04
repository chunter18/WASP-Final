#include <unistd.h>
#include <ctype.h>

#include "wiced.h"
#include "wiced_log.h"
#include "wiced_tcpip.h"
#include "platform.h"
#include "resources.h"
#include "internal/wwd_sdpcm.h"
#include "wiced_dct_common.h"

#include "../headers/ota2_base.h"
#include "../headers/ota2_helper.h"

#ifdef WWD_TEST_NVRAM_OVERRIDE
#include "internal/bus_protocols/wwd_bus_protocol_interface.h"
#endif

#include "wiced_ota2_image.h"
#include "wiced_ota2_service.h"
#include "wiced_ota2_network.h"
#include "../../WICED/internal/wiced_internal_api.h"

#define CHECK_IOCTL_BUFFER( buff )  if ( buff == NULL ) {  wiced_assert("Allocation failed\n", 0 == 1); return WWD_BUFFER_ALLOC_FAIL; }
#define CHECK_RETURN( expr )  { wwd_result_t check_res = (expr); if ( check_res != WWD_SUCCESS ) { wiced_assert("Command failed\n", 0 == 1); return check_res; } }

#define MY_DEVICE_NAME                      "WASP"
#define MY_DEVICE_MODEL                     "0.1"
#define FIRMWARE_VERSION                    "wiced-6.x"

#define NUM_NSECONDS_IN_SECOND                      (1000000000LL)
#define NUM_USECONDS_IN_SECOND                      (1000000)
#define NUM_NSECONDS_IN_MSECOND                     (1000000)
#define NUM_NSECONDS_IN_USECOND                     (1000)
#define NUM_USECONDS_IN_MSECOND                     (1000)
#define NUM_MSECONDS_IN_SECOND                      (1000)

static wiced_time_t ota2_test_start_time;

#define WICED_OTA2_BUFFER_NODE_COUNT         (256)
#define RESTORE_DCT_APP_SETTINGS             (1)

#ifdef PLATFORM_L1_CACHE_BYTES
#define NUM_BUFFERS_POOL_SIZE(x)       ((WICED_LINK_MTU_ALIGNED + sizeof(wiced_packet_t) + 1) * (x))
#define APP_RX_BUFFER_POOL_SIZE        NUM_BUFFERS_POOL_SIZE(WICED_OTA2_BUFFER_NODE_COUNT)
#endif

#ifdef PLATFORM_L1_CACHE_BYTES
uint8_t                          ota2_rx_packets[APP_RX_BUFFER_POOL_SIZE + PLATFORM_L1_CACHE_BYTES]        __attribute__ ((section (".external_ram")));
#else
uint8_t                          ota2_rx_packets[WICED_NETWORK_MTU_SIZE * WICED_OTA2_BUFFER_NODE_COUNT]     __attribute__ ((section (".external_ram")));
#endif

uint8_t ota2_thread_stack_buffer[OTA2_THREAD_STACK_SIZE]                               __attribute__ ((section (".bss.ccm")));


/* template for HTTP GET */
char ota2_get_request_template[] =
{
    "GET %s HTTP/1.1\r\n"
    "Host: %s%s \r\n"
    "\r\n"
};

const char* firmware_version = FIRMWARE_VERSION;

//functions
static ota2_data_t* init_player(void); //main inits
int ota2_log_output_handler(WICED_LOG_LEVEL_T level, char *logmsg); //writes log to stdout
static wiced_result_t log_get_time(wiced_time_t* time); //gets a time for the log prints
wiced_result_t over_the_air_2_app_restore_settings_after_update(ota2_data_t* player, ota2_boot_type_t boot_type);

static void ota2_test_shutdown(ota2_data_t *player);

wiced_result_t ota2_stop_timed_update(ota2_data_t *player);

static void ota2_mainloop(ota2_data_t *player);

wiced_result_t ota2_get_update(ota2_data_t* player);

wiced_result_t my_ota2_callback(void* session_id, wiced_ota2_service_status_t status, uint32_t value, void* opaque );

void ota_update(void); //top level function, only thing wasp app should call

void ota_update(void)
{
    ota2_data_t*   player;

    /*
     * Main initialization.
     */

    if ((player = init_player()) == NULL)
    {
        return;
    }


    ota2_data_t *g_player;
    g_player = player;
    wiced_time_get_time( &g_player->start_time );

    //setting log level to constant 5
    g_player->log_level = 5;
    wiced_log_set_all_levels(5);

    ota2_dct_t* dct_app = NULL;

    if ( wiced_dct_read_lock( (void**) &dct_app, WICED_FALSE, DCT_APP_SECTION, 0, sizeof( *dct_app ) ) != WICED_SUCCESS )
    {
        return;
    }

    int n = sizeof((char*)((ota2_dct_t*)dct_app)->host);
    char *server = (char*) malloc((n)*sizeof(char));
    strcpy(server, (char*)((ota2_dct_t*)dct_app)->host);

    wiced_dct_read_unlock( dct_app, WICED_FALSE );

    memset(g_player->uri_to_stream, 0, sizeof(g_player->uri_to_stream));
    strlcpy(g_player->uri_to_stream, server, (sizeof(g_player->uri_to_stream) - 1) );

    uint32_t event = PLAYER_EVENT_GET_UPDATE;
    wiced_rtos_set_event_flags(&g_player->events, event);

    //main loop goes here
    //ota2_mainloop(player); //this doesnt do much anymore
    //just gonna take the relevant code

    wiced_result_t      result;
    wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "PLAYER_EVENT_GET_UPDATE ! \r\n");
    result = ota2_get_update(player);
    if (result != WICED_SUCCESS)
    {
            wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "PLAYER_EVENT_GET_UPDATE ota2_test_get_update() failed! %d \r\n", result);
    }
    else
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "PLAYER_EVENT_GET_UPDATE ota2_test_get_update() done.\r\n");
    }

    /*
     * Cleanup and exit.
     */

    g_player = NULL;
    ota2_test_shutdown(player); //work is right here
    player = NULL;

}

static ota2_data_t* init_player(void)
{
    ota2_data_t*            player = NULL;
    wiced_result_t          result;
    uint32_t                tag;
    ota2_boot_type_t        boot_type;

    tag = PLAYER_TAG_VALID;

    /*
     * Initialize the logging subsystem.
     */

    result = wiced_log_init(WICED_LOG_ERR, ota2_log_output_handler, log_get_time);
    //both those callback funtions are defined in this file
    if (result != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("wiced_log_init() failed !\r\n"));
    }

    /* Initialize the device */
    result = wiced_init();
    if (result != WICED_SUCCESS)
    {
        return NULL;
    }

   /*
     * Allocate the main data structure.
     */
    player = calloc_named("ota2_test", 1, sizeof(ota2_data_t));
    if (player == NULL)
    {
        wiced_log_printf( "Unable to allocate player structure\r\n");
        return NULL;
    }

    /*
    //console stuff is here - im removing it

    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "Start the command console\r\n");
    result = command_console_init(STDIO_UART, sizeof(ota2_command_buffer), ota2_command_buffer, CONSOLE_COMMAND_HISTORY_LENGTH, ota2_command_history_buffer, " ");
    if (result != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_DEBUG0, "Error starting the command console\r\n");
        free(player);
        return NULL;
    }
    console_add_cmd_table(ota2_command_table);
    console_dct_register_callback(ota2_test_console_dct_callback, player);
    */

    /*
     * Create our event flags.
     */
    result = wiced_rtos_init_event_flags(&player->events);
    if (result != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_DEBUG0, "Error initializing event flags\r\n");
        tag = PLAYER_TAG_INVALID;
    }

    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "\nOTA2_IMAGE_FLASH_BASE                0x%08lx\n", (uint32_t)OTA2_IMAGE_FLASH_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_FACTORY_RESET_AREA_BASE   0x%08lx\n", (uint32_t)OTA2_IMAGE_FACTORY_RESET_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_FAILSAFE_APP_AREA_BASE    0x%08lx\n", (uint32_t)OTA2_IMAGE_FAILSAFE_APP_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_APP_DCT_SAVE_AREA_BASE    0x%08lx\n", (uint32_t)OTA2_IMAGE_APP_DCT_SAVE_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_CURR_LUT_AREA_BASE        0x%08lx\n", (uint32_t)OTA2_IMAGE_CURR_LUT_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_CURR_DCT_1_AREA_BASE      0x%08lx\n", (uint32_t)OTA2_IMAGE_CURR_DCT_1_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_CURR_DCT_2_AREA_BASE      0x%08lx\n", (uint32_t)OTA2_IMAGE_CURR_DCT_2_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_CURR_OTA_APP_AREA_BASE    0x%08lx\n", (uint32_t)OTA2_IMAGE_CURR_OTA_APP_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_CURR_FS_AREA_BASE         0x%08lx\n", (uint32_t)OTA2_IMAGE_CURR_FS_AREA_BASE);
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_CURR_APP0_AREA_BASE       0x%08lx\n", (uint32_t)OTA2_IMAGE_CURR_APP0_AREA_BASE);
#if defined(OTA2_IMAGE_LAST_KNOWN_GOOD_AREA_BASE)
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_LAST_KNOWN_GOOD_AREA_BASE 0x%08lx\n", (uint32_t)OTA2_IMAGE_LAST_KNOWN_GOOD_AREA_BASE);
#endif
    wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "OTA2_IMAGE_STAGING_AREA_BASE         0x%08lx\r\n", (uint32_t)OTA2_IMAGE_STAGING_AREA_BASE);

    /* read in our configurations */
    ota2_config_init(player); //ota2_helper

    /* determine if this is a first boot, factory reset, or after an update boot */
    boot_type = wiced_ota2_get_boot_type();
    switch( boot_type )
    {
        case OTA2_BOOT_FAILSAFE_FACTORY_RESET:
        case OTA2_BOOT_FAILSAFE_UPDATE:
        default:
            /* We should never get here! */
            wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "Unexpected boot_type %d!\r\n", boot_type);
            /* FALL THROUGH */
        case OTA2_BOOT_NEVER_RUN_BEFORE:
            wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "First BOOT EVER\r\n");
            /* Set the reboot type back to normal so we don't think we updated next reboot */
            wiced_dct_ota2_save_copy( OTA2_BOOT_NORMAL );
            break;
        case OTA2_BOOT_NORMAL:
            wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "Normal reboot - count:%ld.\r\n", player->dct_app->reboot_count);
            break;
        case OTA2_BOOT_EXTRACT_FACTORY_RESET:   /* pre-OTA2 failsafe ota2_bootloader designation for OTA2_BOOT_FACTORY_RESET */
        case OTA2_BOOT_FACTORY_RESET:
            wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "Factory Reset Occurred!\r\n");
#if RESTORE_DCT_APP_SETTINGS
            over_the_air_2_app_restore_settings_after_update(player, boot_type);
#endif
            /* Set the reboot type back to normal so we don't think we updated next reboot */
            wiced_dct_ota2_save_copy( OTA2_BOOT_NORMAL );
            break;
        case OTA2_BOOT_EXTRACT_UPDATE:   /* pre-OTA2 failsafe ota2_bootloader designation for OTA2_BOOT_UPDATE */
        case OTA2_BOOT_SOFTAP_UPDATE:    /* pre-OTA2 failsafe ota2_bootloader designation for a SOFTAP update */
        case OTA2_BOOT_UPDATE:
            wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "Update Occurred!\r\n");
#if RESTORE_DCT_APP_SETTINGS
            over_the_air_2_app_restore_settings_after_update(player, boot_type);
#endif
            /* Set the reboot type back to normal so we don't think we updated next reboot */
            wiced_dct_ota2_save_copy( OTA2_BOOT_NORMAL );
            break;
        case OTA2_BOOT_LAST_KNOWN_GOOD:
            wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "Last Known Good used!\r\n");
            break;
    }

    /* Get RTC Clock time and set it here */
    {
        wiced_time_t time = 0;
        wiced_time_set_time( &time );
    }


    /* keep track of # of reboots */
    player->dct_app->reboot_count++;
    wiced_dct_write( (void*)player->dct_app, DCT_APP_SECTION, 0, sizeof( ota2_dct_t ) );

    /* print out our current configuration */
    //this print is annoying
    //ota2_config_print_info(player);

    /* connect to the network */
    player->ota2_bg_params.ota2_interface           = player->dct_network->interface;
    result = wiced_ota2_network_up(player->ota2_bg_params.ota2_interface, player->dct_wifi->stored_ap_list);
    if (result != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "init: Bringing up network interface failed!\r\n");
    }

    /* set our valid tag */
    player->tag = tag;

    return player;
}

int ota2_log_output_handler(WICED_LOG_LEVEL_T level, char *logmsg)
{
    write(STDOUT_FILENO, logmsg, strlen(logmsg));

    return 0;
}

static wiced_result_t log_get_time(wiced_time_t* time)
{
    wiced_time_t now;
    wiced_result_t result;

    /*
     * Get the current time.
     */

    result = wiced_time_get_time(&now);
    *time  = now - ota2_test_start_time;

    return result;
}

wiced_result_t over_the_air_2_app_restore_settings_after_update(ota2_data_t* player, ota2_boot_type_t boot_type)
{
    uint16_t major = 0, minor = 0;
    platform_dct_network_config_t   dct_network = { 0 };
    platform_dct_wifi_config_t      dct_wifi = { 0 };
    ota2_dct_t                      dct_app = { 0 };

    /* read in our configurations from the DCT copy */
    /* network */
    if (wiced_dct_ota2_read_saved_copy( &dct_network, DCT_NETWORK_CONFIG_SECTION, 0, sizeof(platform_dct_network_config_t)) != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_WARNING,   "over_the_air_2_app_restore_settings_after_update() failed reading Network Config!\r\n");
        return WICED_ERROR;
    }

    /* wifi */
    if (wiced_dct_ota2_read_saved_copy( &dct_wifi, DCT_WIFI_CONFIG_SECTION, 0, sizeof(platform_dct_wifi_config_t)) != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_WARNING,   "over_the_air_2_app_restore_settings_after_update() failed reading WiFi Config!\r\n");
        return WICED_ERROR;
    }

    /* App */
    if (wiced_dct_ota2_read_saved_copy( &dct_app, DCT_APP_SECTION, 0, sizeof(ota2_dct_t)) != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_WARNING,   "over_the_air_2_app_restore_settings_after_update() failed reading App Config!\r\n");
        return WICED_ERROR;
    }

    memcpy(player->dct_network, &dct_network, sizeof(platform_dct_network_config_t));
    memcpy(player->dct_wifi, &dct_wifi, sizeof(platform_dct_wifi_config_t));
    memcpy(player->dct_app, &dct_app, sizeof(ota2_dct_t));

    /* update version number based on boot type */
    switch (boot_type)
    {
        default:
            break;
        case OTA2_BOOT_EXTRACT_FACTORY_RESET:   /* pre-OTA2 failsafe ota2_bootloader designation for OTA2_BOOT_FACTORY_RESET */
        case OTA2_BOOT_FACTORY_RESET:
            if (wiced_ota2_image_get_version( WICED_OTA2_IMAGE_TYPE_FACTORY_RESET_APP, &major, &minor) == WICED_SUCCESS)
            {
                player->dct_app->ota2_major_version = major;
                player->dct_app->ota2_minor_version = minor;
            }
            break;
        case OTA2_BOOT_SOFTAP_UPDATE:
        case OTA2_BOOT_EXTRACT_UPDATE:   /* pre-OTA2 failsafe ota2_bootloader designation for OTA2_BOOT_UPDATE */
        case OTA2_BOOT_UPDATE:
            if (wiced_ota2_image_get_version( WICED_OTA2_IMAGE_TYPE_STAGED, &major, &minor) == WICED_SUCCESS)
            {
                player->dct_app->ota2_major_version = major;
                player->dct_app->ota2_minor_version = minor;
            }
            break;
    }

    /* now, save them all! */
    if (ota2_save_config(player) != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_WARNING, "over_the_air_2_app_restore_settings_after_update() failed Saving Config!\r\n");
        return WICED_ERROR;
    }

    wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "Restored saved Configuration!\r\n");
    return WICED_SUCCESS;
}

static void ota2_test_shutdown(ota2_data_t *player)
{

    /*
     * Shutdown the console.
     *
    //dont need to de-init becuase we never initialized the console
    command_console_deinit();
    */

    ota2_stop_timed_update(player);

    wiced_rtos_deinit_event_flags(&player->events);

    ota2_config_deinit(player);

    player->tag = PLAYER_TAG_INVALID;
    free(player);
}

wiced_result_t ota2_stop_timed_update(ota2_data_t *player)
{
    wiced_result_t                          result;

    result = wiced_ota2_service_deinit(player->ota2_bg_service);
    if (result != WICED_SUCCESS)
    {
        wiced_log_msg( WLF_OTA2, WICED_LOG_WARNING,   "wiced_ota2_service_deinit() returned:%d\r\n", result);
    }
    player->ota2_bg_service = NULL;
    return result;
}

static void ota2_mainloop(ota2_data_t *player)
{
    wiced_result_t      result;
    uint32_t            events;

    wiced_log_msg( WLF_OTA2, WICED_LOG_DEBUG0, "Begin ota2_test mainloop\r\n");
    /*
     * If auto play is set then start off by sending ourselves a play event.
     */

    while (player->tag == PLAYER_TAG_VALID)
    {
        events = 0;

        result = wiced_rtos_wait_for_event_flags(&player->events, PLAYER_ALL_EVENTS, &events,
                                                 WICED_TRUE, WAIT_FOR_ANY_EVENT, WICED_WAIT_FOREVER);
        if (result != WICED_SUCCESS)
        {
            continue;
        }


        //potientially useful call from PLAYER_EVENT_STATUS
        //wiced_ota2_service_status(player->ota2_bg_service

        if (events & PLAYER_EVENT_GET_UPDATE)
        {
            //static wiced_ip_address_t INITIALISER_IPV4_ADDRESS(server_ip, MAKE_IPV4_ADDRESS(192,168,1,164));
            //player->ota2_host_name = server_ip;
            wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "PLAYER_EVENT_GET_UPDATE ! \r\n");
            result = ota2_get_update(player);
            if (result != WICED_SUCCESS)
            {
                    wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "PLAYER_EVENT_GET_UPDATE ota2_test_get_update() failed! %d \r\n", result);
            }
            else
            {
                wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "PLAYER_EVENT_GET_UPDATE ota2_test_get_update() done.\r\n");
            }
        }

        //timed update function
        //result = ota2_test_start_timed_update(player);
        //and corresponding stop function
        //result = ota2_test_stop_timed_update(player);


        //player event factory reset status
        //wiced_ota2_image_validate ( WICED_OTA2_IMAGE_TYPE_FACTORY_RESET_APP );


        //player event factory reset reboot
        //wiced_ota2_force_factory_reset_on_reboot();

        //factory reset now - 3 functions
        //wiced_dct_ota2_save_copy(OTA2_BOOT_FACTORY_RESET);
        //wiced_ota2_image_extract ( WICED_OTA2_IMAGE_TYPE_FACTORY_RESET_APP );
        //wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "Reboot now to see change!\r\n");

        //update now - 3 functions
        //wiced_dct_ota2_save_copy(OTA2_BOOT_UPDATE);
        //wiced_ota2_image_fakery(WICED_OTA2_IMAGE_DOWNLOAD_COMPLETE);
        //wiced_ota2_image_extract ( WICED_OTA2_IMAGE_TYPE_STAGED );

        //event update reboot - mark update to be extracted on next boot
        //wiced_ota2_image_fakery(WICED_OTA2_IMAGE_EXTRACT_ON_NEXT_BOOT);

        //EVENT update status
        //wiced_ota2_image_validate ( WICED_OTA2_IMAGE_TYPE_STAGED );


    }   /* while */

    wiced_log_msg( WLF_OTA2, WICED_LOG_DEBUG0, "End ota2_test mainloop\r\n");
}

wiced_result_t ota2_get_update(ota2_data_t* player)
{
    wiced_result_t result = WICED_ERROR;

    /* get the image from the server & save in staging area */

    wiced_ota2_service_uri_split(player->uri_to_stream, player->ota2_host_name, sizeof(player->ota2_host_name),
                                     player->ota2_file_path, sizeof(player->ota2_file_path), &player->ota2_bg_params.port);

    wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "split uri fine\r\n");
    wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "player host name %d\r\n",player->ota2_host_name );
    player->ota2_bg_params.host_name                = player->ota2_host_name;
    player->ota2_bg_params.file_path                = player->ota2_file_path;

    player->ota2_bg_params.auto_update              = 0;
    player->ota2_bg_params.initial_check_interval   = 5;            /* initial check in 5 seconds */
    player->ota2_bg_params.check_interval           = 10 * 60;      /* 10 minutes - use SECONDS_IN_24_HOURS for 1 day */
    player->ota2_bg_params.retry_check_interval     = SECONDS_PER_MINUTE;   /* minimum retry is 1 minute */
    player->ota2_bg_params.max_retries              = 3;       /* maximum retries per update attempt         */
    player->ota2_bg_params.default_ap_info          = player->dct_wifi->stored_ap_list;
    player->ota2_bg_params.ota2_interface           = player->dct_network->interface;
#ifdef WICED_USE_ETHERNET_INTERFACE
    if (player->ota2_bg_params.ota2_interface == WICED_ETHERNET_INTERFACE)
    {
        if ( wiced_network_is_up( WICED_ETHERNET_INTERFACE) == WICED_FALSE )
        {
            /* Currently not connected to Ethernet, use WiFI */
            player->ota2_bg_params.ota2_interface = WICED_STA_INTERFACE;
        }
    }
#endif
    if (player->ota2_bg_params.ota2_interface != WICED_ETHERNET_INTERFACE)
    {
        player->ota2_bg_params.ota2_ap_info             = NULL;
        player->ota2_bg_params.ota2_ap_list             = &player->dct_wifi->stored_ap_list[0]; /* use the DCT AP list */
        player->ota2_bg_params.ota2_ap_list_count       = CONFIG_AP_LIST_SIZE;
    }

    player->deinit_ota2_bg = WICED_TRUE;
    if (player->ota2_bg_service == NULL)
    {
        player->ota2_bg_service = wiced_ota2_service_init(&player->ota2_bg_params, player);
        wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "ota2_test_get_update() wiced_ota2_service_init() bg_service:%p \r\n", player->ota2_bg_service);
    }
    else
    {
        /* bg service already started - this is OK, just don't deinit at the end of this function */
        player->deinit_ota2_bg = WICED_FALSE;
    }

    if (player->ota2_bg_service != NULL)
    {
        /* add a callback */
        result = wiced_ota2_service_register_callback(player->ota2_bg_service, my_ota2_callback);
        if (result != WICED_SUCCESS)
        {
                wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "ota2_test_get_update register callback failed! %d \r\n", result);
                wiced_ota2_service_deinit(player->ota2_bg_service);
                player->ota2_bg_service = NULL;
        }

        if (player->ota2_bg_service != NULL)
        {
            wiced_log_printf( "Download the OTA Image file - get it NOW!\r\n");
            /* NOTE: THis is a blocking call! */
            result = wiced_ota2_service_check_for_updates(player->ota2_bg_service);
            if (result != WICED_SUCCESS)
            {
                    wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "ota2_test_get_update wiced_ota2_service_check_for_updates() failed! %d \r\n", result);
            }
        }
    }
    return result;
}

wiced_result_t my_ota2_callback(void* session_id, wiced_ota2_service_status_t status, uint32_t value, void* opaque )
{
    ota2_data_t*    player = (ota2_data_t*)opaque;

    UNUSED_PARAMETER(session_id);
    UNUSED_PARAMETER(player);


    switch( status )
    {
    case OTA2_SERVICE_STARTED:      /* Background service has started
                                         * return - None                                                             */
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : SERVE STARTED -----------------------------\r\n");
        break;
    case OTA2_SERVICE_AP_CONNECT_ERROR:
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : AP CONNECT_ERROR -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCESS (not used by service). This is informational \r\n");
        break;

    case OTA2_SERVICE_SERVER_CONNECT_ERROR:
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : SERVER_CONNECT_ERROR -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCESS (not used by service). This is informational \r\n");
        break;

    case OTA2_SERVICE_AP_CONNECTED:
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : AP_CONNECTED -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCESS (not used by service). This is informational \r\n");
        break;

    case OTA2_SERVICE_SERVER_CONNECTED:
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : SERVER_CONNECTED -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCESS (not used by service). This is informational \r\n");
        break;


    case OTA2_SERVICE_CHECK_FOR_UPDATE: /* Time to check for updates.
                                         * return - WICED_SUCCESS = Service will check for update availability
                                         *        - WICED_ERROR   = Application will check for update availability   */
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : CHECK_FOR_UPDATE -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCCESS, let Service do the checking.\r\n");
        return WICED_SUCCESS;

    case OTA2_SERVICE_UPDATE_AVAILABLE: /* Service has contacted server, update is available
                                         * return - WICED_SUCCESS = Application indicating that it wants the
                                         *                           OTA Service to perform the download
                                         *        - WICED_ERROR   = Application indicating that it will perform
                                         *                           the download, the OTA Service will do nothing.  */
    {
        /* the OTA2 header for the update is pointed to by the value argument and is only valid for this function call */
        wiced_ota2_image_header_t* ota2_header;

        ota2_header = (wiced_ota2_image_header_t*)value;

        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : UPDATE_AVAILABLE -----------------------------\r\n");
        /* the OTA2 header for the update is pointed to by the value argument and is only valid for this function call */

        /*
         * In an actual application, the application would look at the headers information and decide if the
         * file on the update server is a newer version that the currently running application.
         *
         * If the application wants the update to continue, it would return WICED_SUCCESS here
         * If not, return WICED_ERROR
         *
         */
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "Current Version %d.%d\r\n", player->dct_app->ota2_major_version,
                                                                      player->dct_app->ota2_minor_version);
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "   OTA2 Version %d.%d\r\n", ota2_header->major_version, ota2_header->minor_version);

#if defined(CHECK_OTA2_UPDATE_VERSION)
        if ((player->dct_app->ota2_major_version > ota2_header->major_version) ||
            ((player->dct_app->ota2_major_version == ota2_header->major_version) &&
             (player->dct_app->ota2_minor_version >= ota2_header->minor_version)) )
        {
            wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "OTA2 Update Version Fail - return ERROR, do not update!\r\n");
            return WICED_ERROR;
        }
#endif
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCCESS, let Service perform the download.\r\n");
        return WICED_SUCCESS;
    }

    case OTA2_SERVICE_DOWNLOAD_STATUS:  /* Download status - value has % complete (0-100)
                                         *   NOTE: This will only occur when Service is performing download
                                         * return - WICED_SUCCESS = Service will continue download
                                         *        - WICED_ERROR   = Service will STOP download and service will
                                         *                          issue OTA2_SERVICE_TIME_TO_UPDATE_ERROR           */
        wiced_log_msg( WLF_OTA2, WICED_LOG_DEBUG0, "my_ota2_callback() OTA2_SERVICE_DOWNLOAD_STATUS %ld %%!\r\n", value);
        return WICED_SUCCESS;

    case OTA2_SERVICE_PERFORM_UPDATE:   /* Download is complete
                                         * return - WICED_SUCCESS = Service will inform Bootloader to extract
                                         *                          and update on next power cycle
                                         *        - WICED_ERROR   = Service will inform Bootloader that download
                                         *                          is complete - Bootloader will NOT extract        */
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : PERFORM_UPDATE -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCCESS, let Service extract update on next reboot.\r\n");
        return WICED_SUCCESS;

    case OTA2_SERVICE_UPDATE_ERROR:     /* There was an error in transmission
                                         * This will only occur if Error during Service performing data transfer
                                         * upon return - if retry_interval is set, Service will use retry_interval
                                         *               else, Service will retry on next check_interval
                                         */
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : UPDATE_ERROR -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCCESS, Service will retry as parameters defined in init.\r\n");
        return WICED_SUCCESS;

    case OTA2_SERVICE_UPDATE_ENDED:     /* All update actions for this check are complete.
                                         * This callback is to allow the application to take any actions when
                                         * the service is done checking / downloading an update
                                         * (succesful, unavailable, or error)
                                         * return - None                                                             */
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : UPDATE_ENDED -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCESS (not used by service). This is informational \r\n");
        if ((value == WICED_SUCCESS) && (player->dct_app->ota2_reboot_after_download != 0))
        {
            wiced_log_msg( WLF_OTA2, WICED_LOG_ERR, "        REBOOTING !!!\r\n");
            wiced_framework_reboot();
        }

        if (player->deinit_ota2_bg == WICED_TRUE)
        {
            wiced_rtos_set_event_flags(&player->events, PLAYER_EVENT_STOP_TIMED_UPDATE);
        }
        break;

    case OTA2_SERVICE_STOPPED:
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "----------------------------- OTA2 Service Called : SERVICE ENDED -----------------------------\r\n");
        wiced_log_msg( WLF_OTA2, WICED_LOG_NOTICE, "        return SUCESS (not used by service). This is informational \r\n");
        break;

    default:
        wiced_log_msg( WLF_OTA2, WICED_LOG_INFO, "my_ota2_callback() UNKNOWN STATUS %d!\r\n", status);
        break;
    }

    return WICED_SUCCESS;
}
