#include "esp_zigbee_core.h"
#include "zcl_utility.h"


#define MAX_CHILDREN                      10                                  /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE         false                               /* enable the install code policy for security */
#define ENDPOINT_ID                       10                                  /* device endpoint */
#define ESP_ZB_TOUCHLINK_RSSI_THRESHOLD   -101                                /* Touchlink RSSI threshold */
#define ESP_ZB_PRIMARY_CHANNEL_MASK       ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK                         /* Zigbee primary channel mask use in the example */
#define ESP_ZB_SECONDARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK
#define ESP_ZB_CHANNEL_MASK               ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK

#define ESP_ZB_SECUR_MIN_LQI               0                                      
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
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


static const char * nwk_ind_name[] = {
    [ESP_ZB_NWK_COMMAND_STATUS_NO_ROUTE_AVAILABLE]          = "No route available",
    [ESP_ZB_NWK_COMMAND_STATUS_TREE_LINK_FAILURE]           = "Tree link failure",
    [ESP_ZB_NWK_COMMAND_STATUS_NONE_TREE_LINK_FAILURE]      = "None-tree link failure",
    [ESP_ZB_NWK_COMMAND_STATUS_LOW_BATTERY_LEVEL]           = "Low battery level",
    [ESP_ZB_NWK_COMMAND_STATUS_NO_ROUTING_CAPACITY]         = "No routing capacity",
    [ESP_ZB_NWK_COMMAND_STATUS_NO_INDIRECT_CAPACITY]        = "No indirect capacity",
    [ESP_ZB_NWK_COMMAND_STATUS_INDIRECT_TRANSACTION_EXPIRY] = "Indirect transaction expiry",
    [ESP_ZB_NWK_COMMAND_STATUS_TARGET_DEVICE_UNAVAILABLE]   = "Target device unavailable",
    [ESP_ZB_NWK_COMMAND_STATUS_TARGET_ADDRESS_UNALLOCATED]  = "Target address unallocated",
    [ESP_ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE]         = "Parent link failure",
    [ESP_ZB_NWK_COMMAND_STATUS_VALIDATE_ROUTE]              = "Validate route",
    [ESP_ZB_NWK_COMMAND_STATUS_SOURCE_ROUTE_FAILURE]        = "Source route failure",
    [ESP_ZB_NWK_COMMAND_STATUS_MANY_TO_ONE_ROUTE_FAILURE]   = "Many-to-one route failure",
    [ESP_ZB_NWK_COMMAND_STATUS_ADDRESS_CONFLICT]            = "Address conflict",
    [ESP_ZB_NWK_COMMAND_STATUS_VERIFY_ADDRESS]              = "Verify address",
    [ESP_ZB_NWK_COMMAND_STATUS_PAN_IDENTIFIER_UPDATE]       = "Pan ID update",
    [ESP_ZB_NWK_COMMAND_STATUS_NETWORK_ADDRESS_UPDATE]      = "Network address update",
    [ESP_ZB_NWK_COMMAND_STATUS_BAD_FRAME_COUNTER]           = "Bad frame counter",
    [ESP_ZB_NWK_COMMAND_STATUS_BAD_KEY_SEQUENCE_NUMBER]     = "Bad key sequence number", 
    [ESP_ZB_NWK_COMMAND_STATUS_UNKNOWN_COMMAND]             = "Command received is not known",
};