#pragma once

#ifdef __cplusplus
extern "C" {
#endif


wiced_result_t ota2_save_app_dct(ota2_data_t* player);
wiced_result_t ota2_save_network_dct(ota2_data_t* player);
wiced_result_t ota2_save_wifi_dct(ota2_data_t* player);
wiced_result_t ota2_config_load_dct_wifi(ota2_data_t* player);
wiced_result_t ota2_config_load_dct_network(ota2_data_t* player);

wiced_result_t ota2_config_unload_dct_wifi(ota2_data_t* player);
wiced_result_t ota2_config_unload_dct_network(ota2_data_t* player);

//void ota2_set_config(ota2_data_t* player, int argc, char *argv[]);

//void ota2_config_print_info(ota2_data_t* player);

wiced_result_t ota2_save_config(ota2_data_t* player);

wiced_result_t ota2_config_init(ota2_data_t* player); //done


wiced_result_t ota2_config_deinit(ota2_data_t* player);

//wiced_result_t ota2_config_reload_dct_wifi(ota2_data_t* player);
//wiced_result_t ota2_config_reload_dct_network(ota2_data_t* player);


#ifdef __cplusplus
} /* extern "C" */
#endif
