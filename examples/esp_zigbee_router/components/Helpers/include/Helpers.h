#include "zcl/esp_zigbee_zcl_common.h"
#include "nwk/esp_zigbee_nwk.h"
#include "aps/esp_zigbee_aps.h"
#include "esp_zigbee_core.h"
#include "switch_driver.h"

static uint16_t PAYLOAD_SIZE = (1600);
static uint16_t REPEATS = 100;
static uint16_t DEST_ADDR = 0x0000;
static uint32_t DELAY_MS = 1000;
static uint32_t DELAY_TICK = 10;

bool deferred_driver_init(void);
bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind);
void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm);
void esp_zigbee_include_show_tables(void);
void beacon_task(void *pvParameter);
static uint8_t actions_count = 0;


static const char rel_name[] = {
        [ESP_ZB_NWK_RELATIONSHIP_PARENT]                = 'P', /* Parent */
        [ESP_ZB_NWK_RELATIONSHIP_CHILD]                 = 'C', /* Child */
        [ESP_ZB_NWK_RELATIONSHIP_SIBLING]               = 'S', /* Sibling */
        [ESP_ZB_NWK_RELATIONSHIP_NONE_OF_THE_ABOVE]     = 'O', /* Others */
        [ESP_ZB_NWK_RELATIONSHIP_PREVIOUS_CHILD]        = 'c', /* Previous Child */
        [ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD] = 'u', /* Unauthenticated Child */
    };

static const char *dev_type_name[] = {
     [ESP_ZB_DEVICE_TYPE_COORDINATOR] = "ZC",
     [ESP_ZB_DEVICE_TYPE_ROUTER]      = "ZR",
     [ESP_ZB_DEVICE_TYPE_ED]          = "ZED",
     [ESP_ZB_DEVICE_TYPE_NONE]        = "UNK",
 };

static const char *route_state_name[] = {
    [ESP_ZB_NWK_ROUTE_STATE_ACTIVE] = "Active",
    [ESP_ZB_NWK_ROUTE_STATE_DISCOVERY_UNDERWAY] = "Disc",
    [ESP_ZB_NWK_ROUTE_STATE_DISCOVERY_FAILED] = "Fail",
    [ESP_ZB_NWK_ROUTE_STATE_INACTIVE] = "Inactive",
};

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};

// static const char *write_attr_status_name[] = {
//     [ESP_ZB_ZCL_STATUS_SUCCESS] = "Success",
//     [ESP_ZB_ZCL_STATUS_FAILURE] = "Failure",
//     [ESP_ZB_ZCL_STATUS_NOT_AUTHORIZED] = "Not Authorized",
//     [ESP_ZB_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE] = "Unsupported Attribute",
//     [ESP_ZB_ZCL_STATUS_INVALID_VALUE] = "Invalid Value",
//     [ESP_ZB_ZCL_STATUS_INSUFFICIENT_SPACE] = "Insufficient Space",
//     [ESP_ZB_ZCL_STATUS_NOT_FOUND] = "Not Found",
// };

typedef struct neighbor_info_s{
    uint16_t short_addr;
    uint64_t ieee_addr;
    uint8_t device_type;
    uint8_t relationship;
    uint8_t depth;
    uint8_t lqi;
    uint8_t outgoing_cost;
    uint8_t rssi;
} neighbor_info_t;

typedef struct route_info_s {
    uint16_t dest_addr;
    uint16_t next_hop;
    uint8_t flags;
} route_info_t;
typedef struct topology_report_s {
    esp_zb_ieee_addr_t source_ieee_addr;
    int16_t neighbor_count;
    neighbor_info_t neighbors[10];
    int16_t routes_count;
    route_info_t routes[10];
} topology_report_t;

typedef struct ping_payload_s {
    uint32_t seq_num;
    uint32_t send_time;
    uint32_t max_ping_count;
    uint8_t* payload;
} ping_payload_t;

typedef struct {
    uint16_t new_dest_addr;
    uint32_t new_delay_ms;      // przerwa miedzy żądaniami dla wysyłania sekwancji pingow przez urządzenie końcowe
    uint16_t payload_size;
    int8_t tx_power;
} setting_change_t;

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
