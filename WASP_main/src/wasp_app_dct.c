#include "../headers/wasp_app_dct.h"
#include "wiced_framework.h"

/*
//default vlaues for wasp app dct on first boot after being programmed.
//needed to merge with ota dct section
DEFINE_APP_DCT(dct_read_write_app_dct_t)
{
    .ota_boot_flag      = 0,
    .unused             = 0,
    .host               = "WASP defualt string"
};
*/

DEFINE_APP_DCT(ota2_dct_t)
{
    .reboot_count       = 0,
    .ota2_reboot_after_download = 1, //cole add
    .ota2_major_version = 1,
    .ota2_minor_version = 0,
    //.ota2_update_uri    = "192.165.100.226/OTA2_image_file.bin"
    .ota_boot_flag      = 0,
    .unused             = 0,
    .host               = "WASP defualt string"
};
