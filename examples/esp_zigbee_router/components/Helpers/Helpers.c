#include <stdio.h>
#include "Helpers.h"

#include "esp_check.h"
#include "esp_log.h"

#include "zcl/esp_zigbee_zcl_common.h"
#include "switch_driver.h"

static const char *TAG_include = "esp_zigbee_include";

void send_topology_report(void);

void send_topology_report(){
    topology_report_t report = {0};
    report.neighbor_count = 0;
    report.routes_count = 0;
    esp_zb_get_long_address(report.source_ieee_addr);
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        if (neighbor.device_type == ESP_ZB_DEVICE_TYPE_ROUTER) {
            report.neighbors[report.neighbor_count].short_addr = neighbor.short_addr;
            report.neighbors[report.neighbor_count].ieee_addr = *(uint64_t *)neighbor.ieee_addr;
            report.neighbors[report.neighbor_count].device_type = neighbor.device_type;
            report.neighbors[report.neighbor_count].relationship = neighbor.relationship;
            report.neighbors[report.neighbor_count].depth = neighbor.depth;
            report.neighbors[report.neighbor_count].lqi = neighbor.lqi;
            report.neighbors[report.neighbor_count].outgoing_cost = neighbor.outgoing_cost;
            report.neighbors[report.neighbor_count].rssi = neighbor.rssi;
            report.neighbor_count++;
        }
    }
    itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_info_t route = {};
    while (ESP_OK == esp_zb_nwk_get_next_route(&itor, &route)){
        report.routes[report.routes_count].dest_addr = route.dest_addr;
        report.routes[report.routes_count].next_hop = route.next_hop_addr;
        report.routes[report.routes_count].flags = (* (uint8_t *)&route.flags);
        report.routes_count++;
    }
    
    esp_zb_apsde_data_req_t req  ={
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .dst_addr.addr_short = 0x0000, //Coordinator address
        .dst_endpoint = 32,
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .cluster_id = 0xFF01, //custom cluster
        .src_endpoint = 32,
        .asdu_length = sizeof(report),
        .asdu = (uint8_t *)&report,
        .tx_options = ESP_ZB_APSDE_TX_OPT_ACK_TX | ESP_ZB_APSDE_TX_OPT_FRAG_PERMITTED,
        .use_alias = false,
        .alias_src_addr = 0,
        .alias_seq_num = 0,
        .radius = 5
    };
    ESP_LOGI(TAG_include, "Sending topology report to coordinator");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_aps_data_request(&req);
    esp_zb_lock_release();
    
}
bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind)
{
    actions_count++;
    bool processed = false;
    if (ind.status == 0x00) {
        if (ind.dst_endpoint == 10 && ind.profile_id == ESP_ZB_AF_HA_PROFILE_ID && ind.cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
            ESP_LOGI("APSDE INDICATION",
                    "Received APSDE-DATA %s request with a length of %ld from endpoint %d, source address 0x%04hx to "
                    "endpoint %d, destination address 0x%04hx, rx_time %d, lqi %d, security status %s",
                    ind.dst_addr_mode == 0x01 ? "group" : "unicast", ind.asdu_length, ind.src_endpoint,
                    ind.src_short_addr, ind.dst_endpoint, ind.dst_short_addr, ind.rx_time, ind.lqi,
                    ind.security_status == 0 ? "unsecured network" : "secured network");
            //ESP_LOG_BUFFER_HEX_LEVEL("APSDE INDICATION", ind.asdu, ind.asdu_length, ESP_LOG_INFO);
            processed = true;
        }
        if(ind.dst_endpoint == 32){
            send_topology_report();
        }

    } else {
        ESP_LOGE("APSDE INDICATION", "Invalid status of APSDE-DATA indication, error code: %d", ind.status);
        processed = false;
    }
    return processed;
}

//wyświetla sąsiadów
static void esp_show_neighbor_table()
{

    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};

    ESP_LOGI(TAG_include,"ZigBee Network Neighbors:");
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        ESP_LOGI(TAG_include,"Index: %3d", itor);
        ESP_LOGI(TAG_include,"  Age: %3d", neighbor.age);
        ESP_LOGI(TAG_include,"  Neighbor: 0x%04hx", neighbor.short_addr);
        ESP_LOGI(TAG_include,"  IEEE: 0x%016" PRIx64, *(uint64_t *)neighbor.ieee_addr);
        ESP_LOGI(TAG_include,"  Type: %3s", dev_type_name[neighbor.device_type]);   
        ESP_LOGI(TAG_include,"  Rel: %c", rel_name[neighbor.relationship]);
        ESP_LOGI(TAG_include,"  Depth: %3d", neighbor.depth);
        ESP_LOGI(TAG_include,"  LQI: %3d", neighbor.lqi);
        ESP_LOGI(TAG_include,"  Cost: o:%d", neighbor.outgoing_cost);

    }
    ESP_LOGI(TAG_include," ");
}

//wyswietla trasy
void esp_show_route_table()
{

    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_info_t route = {};

    ESP_LOGI(TAG_include, "Zigbee Network Routes:");
    while (ESP_OK == esp_zb_nwk_get_next_route(&itor, &route)) {
        ESP_LOGI(TAG_include,"Index: %3d", itor);
        ESP_LOGI(TAG_include, "  DestAddr: 0x%04hx", route.dest_addr);
        ESP_LOGI(TAG_include, "  NextHop: 0x%04hx", route.next_hop_addr);
        ESP_LOGI(TAG_include, "  Expiry: %4d", route.expiry);
        ESP_LOGI(TAG_include, "  State: %6s", route_state_name[route.flags.status]);
        ESP_LOGI(TAG_include, "  Flags: 0x%02x", *(uint8_t *)&route.flags);
    }
    ESP_LOGI(TAG_include," ");

}

void esp_zigbee_include_show_tables(void) 
{
    ESP_LOGI(TAG_include, "Zigbee Network Tables:");
    esp_show_neighbor_table();
    esp_show_route_table();
}

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};

void button_handler(switch_func_pair_t *button_func_pair)
{
    if(button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        esp_zigbee_include_show_tables();
        send_topology_report();
        esp_zb_bdb_open_network(30);
    }
}

bool deferred_driver_init(void)
{
    uint8_t button_num = PAIR_SIZE(button_func_pair);
    bool is_initialized = switch_driver_init(button_func_pair, button_num, button_handler);
    return is_initialized;
}

int16_t esp_zigbee_get_router_neightbor_count(void)
{
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};
    int16_t count = 0;

    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        if (neighbor.device_type == ESP_ZB_DEVICE_TYPE_ROUTER) {
            count++;
        }
    }
    return count;
}


