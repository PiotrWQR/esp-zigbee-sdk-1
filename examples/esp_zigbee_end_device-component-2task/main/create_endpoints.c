#include "esp_zigbee_type.h"
#include "esp_zigbee_core.h"

/* Basic manufacturer information */
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x07""esp-c6" /* Customized model identifier */


static void create_test_endpoint(esp_zb_ep_list_t *ep_list, esp_zb_attribute_list_t *basic_cluster, esp_zb_cluster_list_t *cluster_list);
static void create_traffic_manager_endpoint(esp_zb_ep_list_t *ep_list, esp_zb_attribute_list_t *basic_cluster, esp_zb_cluster_list_t *cluster_list);

 
#include "esp_check.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_zigbee_type.h"


static void create_test_endpoint(esp_zb_ep_list_t *ep_list, esp_zb_attribute_list_t *basic_cluster, esp_zb_cluster_list_t *cluster_list)
{
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = 10,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEST_DEVICE_ID,
        .app_device_version = 0,
    };

    /* Added attributes */
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)ESP_MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)ESP_MODEL_IDENTIFIER));
    /* Added clusters */
    /* Added endpoints */
    ESP_ERROR_CHECK(esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config));
}

void create_traffic_manager_endpoint(esp_zb_ep_list_t *ep_list, esp_zb_attribute_list_t *basic_cluster, esp_zb_cluster_list_t *cluster_list)
{
    char *esp_manufacturer_name = "\x09""ESPRESSIF";
    char *esp_model_identifier = "esp-c6";
    
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = 70,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = 40,
        .app_device_version = 0,
    };

    

    /* Added attributes */
    // ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,(void *) ESP_MANUFACTURER_NAME));
    // ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *) ESP_MODEL_IDENTIFIER));
    /* Added clusters */
    /* Added endpoints */
    ESP_ERROR_CHECK(esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config));
    /* Register device */

}


    #define CUSTOM_SERVER_ENDPOINT 0x01
    #define CUSTOM_CLIENT_ENDPOINT 0x01
    #define CUSTOM_CLUSTER_ID 0xff00
    #define CUSTOM_COMMAND_RESP 0x0001
static void create_custom_endpoint(esp_zb_ep_list_t *ep_list, esp_zb_attribute_list_t *custom_cluster, esp_zb_cluster_list_t *cluster_list){
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = CUSTOM_SERVER_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 0,
    };
    uint16_t custom_attr1_id = 0x0000;
    uint8_t custom_attr1_value[] = "\x0b""hello world";
    uint16_t custom_attr2_id = 0x0001;
    uint16_t custom_attr2_value = 0x1234;
    // ESP_ERROR_CHECK(esp_zb_cluster_add_manufacturer_attr(custom_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)ESP_MANUFACTURER_NAME));
    // ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(custom_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)ESP_MODEL_IDENTIFIER));
    esp_zb_custom_cluster_add_custom_attr(custom_cluster, custom_attr1_id, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
                                          ESP_ZB_ZCL_ATTR_ACCESS_WRITE_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, custom_attr1_value);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster, custom_attr2_id, ESP_ZB_ZCL_ATTR_TYPE_U16, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
                                          &custom_attr2_value);

    /* Mandatory clusters */
    //esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    //esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    /* Custom cluster */
    esp_zb_cluster_list_add_custom_cluster(cluster_list, custom_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);
}