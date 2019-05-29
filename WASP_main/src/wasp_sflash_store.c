#include "wiced.h"
#include "../headers/wasp_sflash_store.h"
#include "../headers/wasp_app_dct.h"

int is_ota_boot(void)
{
    ota2_dct_t* dct_app = NULL;

    if ( wiced_dct_read_lock( (void**) &dct_app, WICED_FALSE, DCT_APP_SECTION, 0, sizeof( *dct_app ) ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }

    int boot_flag = (unsigned int)((ota2_dct_t*)dct_app)->ota_boot_flag;

    wiced_dct_read_unlock( dct_app, WICED_FALSE );

    return boot_flag;

}

wiced_result_t set_ota_boot(char* host_string) //easy to use wrapper
{
    modify_app_dct(1, 0, host_string);
    return WICED_SUCCESS;
}

wiced_result_t set_non_ota_boot(void)
{
    //blank str to reset entire string
    /*
    char* blank_host_str = {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0',
                            '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0',
                            '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0',
                            '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0',
                            '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
    */
    char* blank_host_str = {'\0'};
    modify_app_dct(0, 0, blank_host_str);
    return WICED_SUCCESS;
}



wiced_result_t modify_app_dct(uint8_t boot_flag, uint32_t unused, char* host_string)
{
    ota2_dct_t*       app_dct                  = NULL;

    /* get the App config section for modifying, any memory allocation required would be done inside wiced_dct_read_lock() */
    wiced_dct_read_lock( (void**) &app_dct, WICED_TRUE, DCT_APP_SECTION, 0, sizeof( *app_dct ) );


    /* Modify host string in app dct */
    strcpy( app_dct->host, host_string );
    wiced_dct_write( (const void*) app_dct, DCT_APP_SECTION, 0, sizeof(ota2_dct_t) );

    /* Modify the ota boot flag */
    app_dct->ota_boot_flag = boot_flag;
    wiced_dct_write( (const void*) &(app_dct->ota_boot_flag), DCT_APP_SECTION, OFFSETOF(ota2_dct_t, ota_boot_flag), sizeof(uint8_t) );

    /* Modify the unused section - here in case someone wants to use it */
    app_dct->unused = unused;
    wiced_dct_write( (const void*) &(app_dct->unused), DCT_APP_SECTION, OFFSETOF(ota2_dct_t, unused), sizeof(uint32_t) );

    /* release the read lock */
    wiced_dct_read_unlock( app_dct, WICED_TRUE );
    return WICED_SUCCESS;
}

wiced_result_t print_app_dct( void )
{
    ota2_dct_t* dct_app = NULL;

    if ( wiced_dct_read_lock( (void**) &dct_app, WICED_FALSE, DCT_APP_SECTION, 0, sizeof( *dct_app ) ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    /* since we passed ptr_is_writable as WICED_FALSE, we are not allowed to write in to memory pointed by dct_security */

    WPRINT_APP_INFO( ( "\r\n----------------------------------------------------------------\r\n\r\n") );

    /* Application Section */
    WPRINT_APP_INFO( ( "Application Section\r\n") );
    WPRINT_APP_INFO( ( "    ota_boot_flag     : %u \r\n", (unsigned int)((ota2_dct_t*)dct_app)->ota_boot_flag  ) );
    WPRINT_APP_INFO( ( "    unused            : %u \r\n", (unsigned int)((ota2_dct_t*)dct_app)->unused ) );
    WPRINT_APP_INFO( ( "    host              : %s \r\n", (char*)((ota2_dct_t*)dct_app)->host ) );

    /* Here ptr_is_writable should be same as what we passed during wiced_dct_read_lock() */
    wiced_dct_read_unlock( dct_app, WICED_FALSE );

    return WICED_SUCCESS;
}
