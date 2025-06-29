#include "esp_zigbee_core.h"
#include "zcl_utility.h"

#define MAX_CHILDREN                      10                                    /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE         false                                 /* enable the install code policy for security */
#define ENDPOINT_ID                       10                                    /* device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK       (1l << 13)                            //ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */

/* Basic manufacturer information */
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF_ESP32-C6"      /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x07"CONFIG_IDF_TARGET /* Customized model identifier */

#define ESP_ZB_ZR_CONFIG()                              \
{                                                                                \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,                               \
    .install_code_policy = INSTALLCODE_POLICY_ENABLE,                            \
    .nwk_cfg.zczr_cfg = {                                                        \
        .max_children = MAX_CHILDREN,                                            \
    },                                                                           \
}

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
{                                                               \
    .radio_mode = ZB_RADIO_MODE_NATIVE,                         \
}

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
{                                                               \
    .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,       \
}
/* Zigbee configuration */