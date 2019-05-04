#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * This is the soft AP used for device configuration
 */
#define CONFIG_AP_SSID       "WICED Config"
#define CONFIG_AP_CHANNEL    1
#define CONFIG_AP_SECURITY   WICED_SECURITY_WPA2_AES_PSK
#define CONFIG_AP_PASSPHRASE "12345678"

/**
 * This is the soft AP available for normal operation (if used)
 */
#define SOFT_AP_SSID         "WICED Device"
#define SOFT_AP_CHANNEL      1
#define SOFT_AP_SECURITY     WICED_SECURITY_WPA2_AES_PSK
#define SOFT_AP_PASSPHRASE   "WICED_PASSPHRASE"


/**
 * This is the default AP the device will connect to (as a client)
 */

#define CLIENT_AP_SSID              "WASP_2.4"
#define CLIENT_AP_PASSPHRASE        "BbBR977KrhAnG33Q"
#define CLIENT_AP_BSS_TYPE          WICED_BSS_TYPE_INFRASTRUCTURE
#define CLIENT_AP_SECURITY          WICED_SECURITY_WPA2_MIXED_PSK
#define CLIENT_AP_CHANNEL           1
#define CLIENT_AP_BAND              WICED_802_11_BAND_2_4GHZ


#define CLIENT_AP_2_SSID            "WASP_5.0"
#define CLIENT_AP_2_PASSPHRASE      "GSf2dSaqJYFf9Akc"
#define CLIENT_AP_2_BSS_TYPE        WICED_BSS_TYPE_INFRASTRUCTURE
#define CLIENT_AP_2_SECURITY        WICED_SECURITY_WPA2_MIXED_PSK
#define CLIENT_AP_2_CHANNEL         1
#define CLIENT_AP_2_BAND            WICED_802_11_BAND_5GHZ

#define CLIENT_AP_3_SSID            "OpenWrtX"
#define CLIENT_AP_3_PASSPHRASE      "seniordesign2019"
#define CLIENT_AP_3_BSS_TYPE        WICED_BSS_TYPE_INFRASTRUCTURE
#define CLIENT_AP_3_SECURITY        WICED_SECURITY_WPA2_MIXED_PSK
#define CLIENT_AP_3_CHANNEL         1
#define CLIENT_AP_3_BAND            WICED_802_11_BAND_2_4GHZ


/**
 * The network interface the device will work with
 */
#define WICED_NETWORK_INTERFACE   WICED_STA_INTERFACE

#ifdef __cplusplus
} /*extern "C" */
#endif
