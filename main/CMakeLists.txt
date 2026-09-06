idf_component_register(SRCS 
							"uart_lcd_touch_example_main.c" 
							"weather_app_gui.c"
							"wind.c"
                       INCLUDE_DIRS "." 
                       REQUIRES 
                       		esp_driver_uart 
                       		esp_wifi 
                       		nvs_flash 
                       		esp_http_client 
                        		esp-tls
                       		spiffs)

include(${CMAKE_CURRENT_LIST_DIR}/lwjson/library.cmake)
target_link_libraries(${COMPONENT_TARGET} PUBLIC lwjson)
                       
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
