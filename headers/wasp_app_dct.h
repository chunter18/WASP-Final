#pragma once

#include <stdint.h>
#include "wiced_ota2_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * define the current version here or on make command line as args
 * make <application>-<platform> ota2_image APP_VERSION_FOR_OTA2_MAJOR=x APP_VERSION_FOR_OTA2_MINOR=y
 */
#if defined(OTA2_SUPPORT)
#ifndef APP_VERSION_FOR_OTA2_MAJOR
#define APP_VERSION_FOR_OTA2_MAJOR  1
#endif
#ifndef APP_VERSION_FOR_OTA2_MINOR
#define APP_VERSION_FOR_OTA2_MINOR  0
#endif
#endif

#pragma pack (1)

typedef struct ota2_dct_s
{
        uint32_t                    reboot_count;
        uint8_t                     ota2_reboot_after_download;                     /* automatic reboot after successful download */
        uint16_t                    ota2_major_version;                             /* define APP_VERSION_FOR_OTA2_MAJOR as make arg */
        uint16_t                    ota2_minor_version;                             /* define APP_VERSION_FOR_OTA2_MINOR as make arg */
        //char                      ota2_update_uri[WICED_OTA2_HTTP_QUERY_SIZE];    /* default Web address of update package */
        uint8_t                     ota_boot_flag;                                  /* flag to tell sys to execute ota functionality */
        uint32_t                    unused;                                         /* unused space, can be used for anything in the future */
        char                        host[50];                                       /* string holding the ip/file target for upgrade, given by server */
} ota2_dct_t;

#pragma pack ()

#ifdef __cplusplus
} /*extern "C" */
#endif
