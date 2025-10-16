idf_component_register(SRCS "Helpers.c"
                    INCLUDE_DIRS "include" "../../../common/zcl_utility/include" "../../managed_components/espressif__esp-zboss-lib/include" "../../managed_components/espressif__esp-zigbee-lib/include" "../../../common/switch_driver/include"
                    REQUIRES "espressif__esp-zboss-lib" "espressif__esp-zigbee-lib" "switch_driver"
)