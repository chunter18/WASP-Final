NAME := WASP_mainapp

$(NAME)_SOURCES := src/main.c
$(NAME)_SOURCES += src/wasp_i2c.c
$(NAME)_SOURCES += src/WASP_LED.c
$(NAME)_SOURCES += src/wasp_hibernate.c
$(NAME)_SOURCES += src/wasp_sflash_store.c
$(NAME)_SOURCES += src/wasp_wifi_switch.c
$(NAME)_SOURCES += src/wasp_tcp.c
$(NAME)_SOURCES += src/wasp_adxl.c
$(NAME)_SOURCES += src/ota2_base.c
$(NAME)_SOURCES += src/ota2_helper.c


$(NAME)_COMPONENTS := protocols/SNTP		   		 \
					  protocols/DNS 				 \
					  protocols/HTTP				 \
                      utilities/wiced_log 		 	 \
                      utilities/mini_printf 		 \
                      daemons/ota2_service		 	 \
                      filesystems/wicedfs			 

WIFI_CONFIG_DCT_H := wifi_config_dct.h
APPLICATION_DCT := src/wasp_app_dct.c
#APPLICATION_DCT    := ota2_test_dct.c will need to integrate this

#OTA SoftAp application - not sure if needed?
OTA_APPLICATION	:= snip.ota2_extract-$(PLATFORM)
OTA_APP    := build/$(OTA_APPLICATION)/binary/$(OTA_APPLICATION).stripped.elf

GLOBAL_DEFINES     += PLATFORM_NO_DDR=1
GLOBAL_DEFINES     += APPLICATION_STACK_SIZE=16000
# stack needs to be big enough to handle the CRC32 calculation buffer
GLOBAL_DEFINES     += DCT_CRC32_CALCULATION_SIZE_ON_STACK=2048
GLOBAL_DEFINES += WWD_TEST_NVRAM_OVERRIDE
GLOBAL_DEFINES += TX_PACKET_POOL_SIZE=5 \
                  RX_PACKET_POOL_SIZE=20 \
                  PBUF_POOL_TX_SIZE=8 \
                  PBUF_POOL_RX_SIZE=8 \
                  WICED_ETHERNET_DESCNUM_TX=32 \
                  WICED_ETHERNET_DESCNUM_RX=8 \
                  WICED_ETHERNET_RX_PACKET_POOL_SIZE=32+WICED_ETHERNET_DESCNUM_RX
                  
#tcp window size is usually 7k, reduce here due to need for reduced host memory usage
#and the host needing the extra processing time
#increase this (and provide more memory) if faster download speeds are desired
GLOBAL_DEFINES += WICED_TCP_WINDOW_SIZE=3072

VALID_PLATFORMS := CYW43907* Quicksilver_EVL
VALID_OSNS_COMBOS := ThreadX-NetX_Duo



# undefine to skip check for Version checking of update while developing updates
# define (and provide version #) for production version testing
# You can provide version # during compilation (default is 0) by adding to build compile args
# <application>-<platform> ota2_image APP_VERSION_FOR_OTA2_MAJOR=x APP_VERSION_FOR_OTA2_MINOR=y
#GLOBAL_DEFINES      += CHECK_OTA2_UPDATE_VERSION=1
#WICED_ENABLE_TRACEX := 1








