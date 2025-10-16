
#include "aps/esp_zigbee_aps.h"
#include "nwk/esp_zigbee_nwk.h"

static uint16_t PAYLOAD_SIZE = (1600);
static uint16_t REPEATS = 100;
static uint16_t DEST_ADDR = 0x0000;
static uint32_t DELAY_MS = 1000;
static uint32_t DELAY_TICK = 10;

bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind);
void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm);
bool deferred_driver_init(void);
void esp_show_neighbor_table();
void esp_show_route_table();
void esp_zigbee_include_show_tables(void);
void beacon_task(void *pvParameters);
void send_traffic_report(void);
void refresh_routes(void);

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

typedef struct {
    uint32_t traffic_count; //Bits received in last 10 seconds
    //esp_zb_ieee_addr_t priority_node
} esp_zb_network_traffic_report_t;

typedef struct data_to_send_s {
    uint32_t start_time;
    uint32_t end_time;
    uint32_t failed_ping_count;
    uint32_t successful_ping_count;
    esp_zb_ieee_addr_t addr;
    uint32_t recon_time;
    uint32_t repeats;
    uint32_t delay;
    uint32_t size;
} data_to_send_t;

#define PING_PAYLOAD_SIZE (PAYLOAD_SIZE - 3*sizeof(uint32_t))
typedef struct ping_payload_s {
    uint32_t seq_num;
    uint32_t send_time;
    uint32_t max_ping_count;
    uint8_t* payload;
} ping_payload_t;


typedef struct {
    uint16_t new_repeats;
    uint16_t new_dest_addr;
    uint32_t new_delay_ms;
    uint32_t new_delay_tick;
    uint8_t csma_min_be;        /*!< The minimum value of the backoff exponent, BE, in the CSMA-CA algorithm. */
    uint8_t csma_max_be;        /*!< The maximum value of the backoff exponent, BE, in the CSMA-CA algorithm. */
    uint8_t csma_max_backoffs;
    uint16_t payload_size;
} setting_change_t;
