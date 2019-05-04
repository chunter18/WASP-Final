#include <ctype.h>
#include "wiced.h"
#include "wiced_log.h"
#include "platform_audio.h"
//#include "command_console.h" //not needed

#include "../headers/ota2_base.h"
#include "../headers/ota2_helper.h"
#include "wiced_ota2_image.h"

#define APP_DCT_OTA2_REBOOT_DIRTY   (1 << 0)
#define APP_DCT_OTA2_URI_DIRTY      (1 << 1)

#define MAC_STR_LEN                     (18)

typedef struct security_lookup_s {
    char     *name;
    wiced_security_t sec_type;
} security_lookup_t;

static uint32_t app_dct_dirty;

static security_lookup_t ota2_security_name_table[] =
{
        { "open",       WICED_SECURITY_OPEN           },
        { "none",       WICED_SECURITY_OPEN           },
        { "wep",        WICED_SECURITY_WEP_PSK        },
        { "shared",     WICED_SECURITY_WEP_SHARED     },
        { "wpatkip",    WICED_SECURITY_WPA_TKIP_PSK   },
        { "wpaaes",     WICED_SECURITY_WPA_AES_PSK    },
        { "wpa2aes",    WICED_SECURITY_WPA2_AES_PSK   },
        { "wpa2tkip",   WICED_SECURITY_WPA2_TKIP_PSK  },
        { "wpa2mix",    WICED_SECURITY_WPA2_MIXED_PSK },
        { "wpa2aesent", WICED_SECURITY_WPA2_AES_ENT   },
        { "ibss",       WICED_SECURITY_IBSS_OPEN      },
        { "wpsopen",    WICED_SECURITY_WPS_OPEN       },
        { "wpsnone",    WICED_SECURITY_WPS_OPEN       },
        { "wpsaes",     WICED_SECURITY_WPS_SECURE     },
        { "invalid",    WICED_SECURITY_UNKNOWN        },
};

static char* ota2_boot_type_string[OTA2_MAX_BOOT_TYPES] =
{
        "OTA2_BOOT_NEVER_RUN_BEFORE",
        "OTA2_BOOT_NORMAL",
        "OTA2_BOOT_EXTRACT_FACTORY_RESET",
        "OTA2_BOOT_EXTRACT_UPDATE",
        "OTA2_BOOT_SOFTAP_UPDATE",
        "OTA2_BOOT_LAST_KNOWN_GOOD",

        "OTA2_BOOT_FACTORY_RESET",
        "OTA2_BOOT_UPDATE",

        "OTA2_BOOT_FAILSAFE_FACTORY_RESET",
        "OTA2_BOOT_FAILSAFE_UPDATE",
};

//static void ota_test_print_app_info(ota2_data_t* player);
//static void ota_test_print_network_info(ota2_data_t* player);
//static void ota_test_print_wifi_info(ota2_data_t* player);

wiced_result_t ota2_config_init(ota2_data_t* player)
{
    wiced_result_t result;

    /* wifi */
    result = ota2_config_load_dct_wifi(player);

    /* network */
    result |= ota2_config_load_dct_network(player);

    /* App */
    result |= wiced_dct_read_lock( (void**)&player->dct_app, WICED_TRUE, DCT_APP_SECTION, 0, sizeof( ota2_dct_t ) );

    return result;
}

wiced_result_t ota2_save_config(ota2_data_t* player)
{
    wiced_result_t result;
    /* save all the configuration info */
    result = ota2_save_app_dct(player);
    if (result != WICED_SUCCESS)
    {
        wiced_log_printf( "ota2_save_config() ota2_save_app_dct() failed:%d\r\n", result);
        return result;
    }
    app_dct_dirty = 0;

    result = ota2_save_network_dct(player);
    if (result != WICED_SUCCESS)
    {
        wiced_log_printf( "ota2_save_config() ota2_save_network_dct() failed:%d\r\n", result);
        return result;
    }
    result = ota2_save_wifi_dct(player);
    if (result != WICED_SUCCESS)
    {
        wiced_log_printf( "ota2_save_config() ota2_save_wifi_dct() failed:%d\r\n", result);
        return result;
    }

    return result;
}


//THESE NEXT THREE ARE USED EXCLUSIVELY BY OTA2_SAVE_CONFIG
wiced_result_t ota2_save_app_dct(ota2_data_t* player)
{
    return wiced_dct_write( (void*)player->dct_app, DCT_APP_SECTION, 0, sizeof(ota2_dct_t) );
}

wiced_result_t ota2_save_network_dct(ota2_data_t* player)
{
    return wiced_dct_write( (void*)player->dct_network, DCT_NETWORK_CONFIG_SECTION, 0, sizeof(platform_dct_network_config_t) );
}

wiced_result_t ota2_save_wifi_dct(ota2_data_t* player)
{
    return wiced_dct_write( (void*)player->dct_wifi, DCT_WIFI_CONFIG_SECTION, 0, sizeof(platform_dct_wifi_config_t) );
}

wiced_result_t ota2_config_load_dct_wifi(ota2_data_t* player)
{
    wiced_result_t result;
    result = wiced_dct_read_lock((void**)&player->dct_wifi, WICED_TRUE, DCT_WIFI_CONFIG_SECTION, 0, sizeof(platform_dct_wifi_config_t));
    if (result != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("Can't get WIFi configuration!\r\n"));
    }
    return result;
}

wiced_result_t ota2_config_load_dct_network(ota2_data_t* player)
{
    wiced_result_t result;
    result = wiced_dct_read_lock( (void**)&player->dct_network, WICED_TRUE, DCT_NETWORK_CONFIG_SECTION, 0, sizeof(platform_dct_network_config_t) );
    if (result != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("Can't get Network configuration!\r\n"));
    }
    return result;
}

wiced_result_t ota2_config_unload_dct_wifi(ota2_data_t* player)
{
    wiced_result_t result = WICED_SUCCESS;

    if ((player != NULL) && (player->dct_wifi != NULL))
    {
        result = wiced_dct_read_unlock(player->dct_wifi, WICED_TRUE);
        if (result == WICED_SUCCESS)
        {
            player->dct_wifi = NULL;
        }
        else
        {
            WPRINT_APP_INFO(("Can't Free/Release WiFi Configuration !\r\n"));
        }
    }
    return result;
}

wiced_result_t ota2_config_unload_dct_network(ota2_data_t* player)
{
    wiced_result_t result = WICED_SUCCESS;

    if ((player != NULL) && (player->dct_network != NULL))
    {
        result = wiced_dct_read_unlock(player->dct_network, WICED_TRUE);
        if (result == WICED_SUCCESS)
        {
            player->dct_network = NULL;
        }
        else
        {
            WPRINT_APP_INFO(("Can't Free/Release Network Configuration !\r\n"));
        }
    }
    return result;
}

wiced_result_t ota2_config_deinit(ota2_data_t* player)
{
    wiced_result_t result;

    result = wiced_dct_read_unlock(player->dct_app, WICED_TRUE);

    result |= ota2_config_unload_dct_network(player);
    result |= ota2_config_unload_dct_wifi(player);


    return result;
}
