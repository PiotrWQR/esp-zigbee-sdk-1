#include "zcl/esp_zigbee_zcl_common.h"
#include "nwk/esp_zigbee_nwk.h"
#include "aps/esp_zigbee_aps.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_type.h"
#include <freertos/queue.h>

#define MIN_BACKOFF                    4                                    /* Minimum value of backoff exponent */
#define MAX_BACKOFF_TIME               5                                    /* Maximum value of backoff exponent */
#define MAX_BACKOFF_RETRIES            4                                    /* Maximum number of backoff retries */

static uint16_t repeats = 40;
static uint16_t dest_addr = 0x0000;
static uint32_t delay_ms = 1000;

bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind);
void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm);


bool deferred_driver_init();
void esp_zigbee_include_show_tables(void);

void send_traffic_report(void);
void refresh_routes(void);
void traffic_reporter_init(void *pvParameters);
void zero_traffic_raport(void);
void send_settings(uint16_t short_addr);

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
    [ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT]             = 37,
    [ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT]             = 9,
    [ESP_ZB_APS_ADDR_MODE_64_PRESENT_ENDP_NOT_PRESENT] = 8,
};
typedef struct esp_zb_network_traffic_raport_s {
    uint16_t short_addr;      //Short address of the reporting device
    bool is_active;
    uint32_t traffic_count; //Packets received
    uint32_t max_ping_count;
    uint32_t last_seq_num;
    uint32_t seq_num;
    uint32_t missed_packets;
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
    uint32_t max_ping_count;
    uint8_t *payload;
} ping_payload_t;

typedef struct {
    uint16_t new_repeats;
    uint16_t new_dest_addr;
    uint32_t new_delay_ms;
    uint32_t new_delay_tick;
    uint8_t csma_min_be;        /*!< The minimum value of the backoff exponent, BE, in the CSMA-CA algorithm. */
    uint8_t csma_max_be;        /*!< The maximum value of the backoff exponent, BE, in the CSMA-CA algorithm. */
    uint8_t csma_max_backoffs;
} setting_change_t;

typedef struct neighbor_info_s{
    uint16_t short_addr;
    uint64_t ieee_addr;
    uint8_t device_type;
    uint8_t relationship;
    uint8_t depth;
    uint8_t lqi;
    uint8_t outgoing_cost;
    int8_t rssi;
} neighbor_info_t;

typedef struct route_info_s {
    uint16_t dest_addr;
    uint16_t next_hop;
    uint8_t flags;
} route_info_t;
typedef struct topology_report_s {
    esp_zb_ieee_addr_t ieee_addr;
    int16_t neighbor_count;
    neighbor_info_t neighbors[10];
    int16_t routes_count;
    route_info_t routes[10];
} topology_report_t;