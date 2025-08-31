#include "zcl/esp_zigbee_zcl_common.h"
#include "nwk/esp_zigbee_nwk.h"
#include "aps/esp_zigbee_aps.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_type.h"
#include <freertos/queue.h>




bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind);
void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm);


bool deferred_driver_init();
void esp_zigbee_include_show_tables(void);

void send_traffic_report(void);
void refresh_routes(void);
void traffic_reporter_init(void *pvParameters);

static const char *dev_type_name[] = {
    [ESP_ZB_DEVICE_TYPE_COORDINATOR] = "ZC",
    [ESP_ZB_DEVICE_TYPE_ROUTER]      = "ZR",
    [ESP_ZB_DEVICE_TYPE_ED]          = "ZED",
    [ESP_ZB_DEVICE_TYPE_NONE]        = "UNK",
};
static const char rel_name[] = {
    [ESP_ZB_NWK_RELATIONSHIP_PARENT]                = 'P', /* Parent */
    [ESP_ZB_NWK_RELATIONSHIP_CHILD]                 = 'C', /* Child */
    [ESP_ZB_NWK_RELATIONSHIP_SIBLING]               = 'S', /* Sibling */
    [ESP_ZB_NWK_RELATIONSHIP_NONE_OF_THE_ABOVE]     = 'O', /* Others */
    [ESP_ZB_NWK_RELATIONSHIP_PREVIOUS_CHILD]        = 'c', /* Previous Child */
    [ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD] = 'u', /* Unauthenticated Child */
};

static const char *route_state_name[] = {
    [ESP_ZB_NWK_ROUTE_STATE_ACTIVE] = "Active",
    [ESP_ZB_NWK_ROUTE_STATE_DISCOVERY_UNDERWAY] = "Disc",
    [ESP_ZB_NWK_ROUTE_STATE_DISCOVERY_FAILED] = "Fail",
    [ESP_ZB_NWK_ROUTE_STATE_INACTIVE] = "Inactive",
};
    
static const uint8_t aps_address_modes_size[] = {
    [ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT]   = 0,
    [ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT]   = 2,
    [ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT]             = 3,
    [ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT]             = 9,
    [ESP_ZB_APS_ADDR_MODE_64_PRESENT_ENDP_NOT_PRESENT] = 8,
};
typedef struct esp_zb_network_traffic_report_s {
    uint16_t short_addr;      //Short address of the reporting device
    uint32_t traffic_count; //Packets received
} esp_zb_network_traffic_raport_t;


typedef struct data_recived_s {
    uint32_t start_time;
    uint32_t end_time;
        uint32_t failed_ping_count;
    uint32_t successful_ping_count;
} data_recived_t;

typedef struct ping_payload_s {
    uint32_t seq_num;
    uint32_t send_time;
    uint8_t *payload;
} ping_payload_t;