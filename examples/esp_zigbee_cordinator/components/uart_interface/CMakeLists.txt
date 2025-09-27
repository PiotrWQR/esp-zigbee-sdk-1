idf_component_register(SRCS "uart_interface.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "console" "driver" "esp_event" "nvs_flash" "freertos" )
