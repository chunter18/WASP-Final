#include "wiced.h"
#include "resources.h"
//NOTE: NEED TO ADD FUCTION TO wwd_wifi.c/.h - see https://community.cypress.com/thread/41629
#include "../headers/wasp_wifi_switch.h"

wiced_result_t switch_networks(int networknum) //2g is 1, 5g is 2
{
    wiced_result_t wiced_res;

    wwd_wifi_disassoc();
    wiced_res = wiced_network_down(WWD_STA_INTERFACE);
    if(wiced_res != WICED_SUCCESS)
        return wiced_res;

    switch(networknum)
    {
        case 1:
            read_write_dct(TWOG_SSID,TWOG_PASS);
            break;
        case 2:
            read_write_dct(FIVEG_SSID,FIVEG_PASS);
            break;
        case 3:
            break; //just in case we want to do 2 bands of 5Ghz
    }

    wiced_res = wiced_network_up(WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL);
    return wiced_res;
}

void read_write_dct(char * apSSID, char * sec_key)
{
    platform_dct_wifi_config_t*     dct_wifi_config          = NULL;

    /* get the wi-fi config section for modifying, any memory allocation required would be done inside wiced_dct_read_lock() */
    wiced_dct_read_lock( (void**) &dct_wifi_config, WICED_TRUE, DCT_WIFI_CONFIG_SECTION, 0, sizeof( *dct_wifi_config ) );


    memset((char *)dct_wifi_config->stored_ap_list[0].details.SSID.value, 0,
            sizeof(dct_wifi_config->stored_ap_list[0].details.SSID.value));
    dct_wifi_config->stored_ap_list[0].details.SSID.length = strlen( apSSID );
    strlcpy((char *)dct_wifi_config->stored_ap_list[0].details.SSID.value,
           apSSID,
            sizeof(dct_wifi_config->stored_ap_list[0].details.SSID.value));

    memset((char *)dct_wifi_config->stored_ap_list[0].security_key, 0,
            sizeof(dct_wifi_config->stored_ap_list[0].security_key));
    dct_wifi_config->stored_ap_list[0].security_key_length = strlen(sec_key );
    strlcpy((char *)dct_wifi_config->stored_ap_list[0].security_key,
            sec_key,
            sizeof(dct_wifi_config->stored_ap_list[0].security_key));

    dct_wifi_config->stored_ap_list[0].details.security = WICED_SECURITY_WPA2_MIXED_PSK;

    wiced_dct_write( (const void*) dct_wifi_config, DCT_WIFI_CONFIG_SECTION, 0, sizeof(platform_dct_wifi_config_t) );


    /* release the read lock */
    wiced_dct_read_unlock( dct_wifi_config, WICED_TRUE );


}
