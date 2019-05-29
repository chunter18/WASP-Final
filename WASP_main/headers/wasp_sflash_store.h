#ifdef USE_APP_CONFIG
static const configuration_entry_t app_config[] =
{
    {"ota_boot_flag", DCT_OFFSET(dct_read_write_app_dct_t, uint8_var ),  4,  CONFIG_UINT32_DATA },
    {"unused", DCT_OFFSET(dct_read_write_app_dct_t, uint32_var),  4,  CONFIG_UINT8_DATA },
    {"host", DCT_OFFSET(dct_read_write_app_dct_t, string_var), 50,  CONFIG_STRING_DATA },
    {0,0,0,0}
};
#endif /* #ifdef USE_APP_CONFIG */


wiced_result_t print_app_dct( void );
wiced_result_t modify_app_dct(uint8_t boot_flag, uint32_t unused, char* host_string);
wiced_result_t set_ota_boot(char* host_string); //easy to use wrapper
wiced_result_t set_non_ota_boot(void);
int is_ota_boot(void);
