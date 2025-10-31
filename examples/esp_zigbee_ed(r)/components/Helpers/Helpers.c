#include <stdio.h>
#include <memory.h>
#include "Helpers.h"
#include "esp_check.h"
#include "esp_log.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "switch_driver.h"
#include "esp_random.h"

static const char *TAG_include = "esp_zigbee_include";
void create_ping_seq(uint16_t dest_addr, uint32_t seq_num);
void send_topology_report(void);

TaskHandle_t beacon_task_handle = NULL;
static uint16_t successful_ping_count = 0;
static uint16_t failed_ping_count = 0;
static uint32_t recon_time = 0;

//wysłanie wiadomości o trasach i sąsiedztwie do koordynatora
void send_topology_report(){
    esp_err_t ret = ESP_OK;
    topology_report_t report = {0};
    report.neighbor_count = 0;
    report.routes_count = 0;
    esp_zb_get_long_address(report.source_ieee_addr);
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
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
    //pre_lock:
    //ESP_GOTO_ON_FALSE(esp_zb_lock_acquire(portMAX_DELAY), 32, pre_lock, TAG_include, "Failed to acquire lock before sending topology report");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_aps_data_request(&req);
    esp_zb_lock_release();
    
}

void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm)
{
    if(confirm.status == 0x00) {
        if(confirm.dst_endpoint == 10) {
            successful_ping_count++;
        }
        ESP_LOGI("APSDE DATA CONFIRM", "Data confirmed successfully");
    } else {
        if(confirm.dst_endpoint == 10) {
            failed_ping_count++;
        }
        ESP_LOGE("APSDE DATA CONFIRM", "Data confirmation failed, error code: %02x", confirm.status);
    }

}

bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind)
{
    actions_count++;
    bool processed = false;
    if (ind.status == 0x00) {
        if(ind.dst_endpoint == 30 && ind.profile_id == ESP_ZB_AF_HA_PROFILE_ID && ind.cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
            setting_change_t *setting_change = (setting_change_t *)ind.asdu;
            ESP_LOGI("APSDE INDICATION", "Received settings change: DEST_ADDR=0x%04hx, DELAY_MS=%ld , TX_POWER=%d, PAYLOAD_SIZE=%d", 
                     setting_change->new_dest_addr, setting_change->new_delay_ms, setting_change->tx_power, setting_change->payload_size);
            DELAY_MS = setting_change->new_delay_ms;
            DEST_ADDR = setting_change->new_dest_addr;
            PAYLOAD_SIZE = setting_change->payload_size;
            esp_zb_set_tx_power(setting_change->tx_power);

            return true;
        }
        if(ind.dst_endpoint == 32){
            send_topology_report(); ///wysłąnie wiadomośći o trasach i sąsiedztwie do koordynatora
            return true;
        }
        if(ind.dst_endpoint == 10 && ind.profile_id == ESP_ZB_AF_HA_PROFILE_ID && ind.cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
            ESP_LOGI("APSDE INDICATION",
                    "Received APSDE-DATA %s request with a length of %ld from endpoint %d, source address 0x%04hx to "
                    "endpoint %d, destination address 0x%04hx, rx_time %d, lqi %d, security status %s",
                    ind.dst_addr_mode == 0x01 ? "group" : "unicast", ind.asdu_length, ind.src_endpoint,
                    ind.src_short_addr, ind.dst_endpoint, ind.dst_short_addr, ind.rx_time, ind.lqi,
                    ind.security_status == 0 ? "unsecured network" : "secured network");
            //ESP_LOG_BUFFER_HEX_LEVEL("APSDE INDICATION", ind.asdu, ind.asdu_length, ESP_LOG_INFO);
            processed = true;
        }
    } else {
        ESP_LOGE("APSDE INDICATION", "Invalid status of APSDE-DATA indication, error code: %d", ind.status);
        processed = false;
    }
    return processed;
}
//wyświetla sąsiadów
void esp_show_neighbor_table()
{
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};
    const char *TAG = "Neighbor Table";

    ESP_LOGI(TAG,"ZigBee Network Neighbors:");
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        ESP_LOGI(TAG,"Index: %3d", itor);
        ESP_LOGI(TAG,"  Age: %3d", neighbor.age);
        ESP_LOGI(TAG,"  Neighbor: 0x%04hx", neighbor.short_addr);
        ESP_LOGI(TAG,"  IEEE: 0x%016" PRIx64, *(uint64_t *)neighbor.ieee_addr);
        ESP_LOGI(TAG,"  Type: %3s", dev_type_name[neighbor.device_type]);   
        ESP_LOGI(TAG,"  Rel: %c", rel_name[neighbor.relationship]);
        ESP_LOGI(TAG,"  Depth: %3d", neighbor.depth);
        ESP_LOGI(TAG,"  LQI: %3d", neighbor.lqi);
        ESP_LOGI(TAG,"  Cost: o:%d", neighbor.outgoing_cost);
    }
    ESP_LOGI(TAG," ");
}
void esp_show_record_route_table()
{   
    esp_zb_nwk_route_record_info_t route_record ={0};
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    const char *TAG = "Record Route Table";

    ESP_LOGI(TAG,"Zigbee Network Record Routes:");
    while (ESP_OK == esp_zb_nwk_get_next_route_record(&itor, &route_record)) {
        ESP_LOGI(TAG,"Index: %3d", itor);
        ESP_LOGI(TAG,"  DestAddr: 0x%04hx", route_record.dest_address);
        ESP_LOGI(TAG,"  NextHop: 0x%04hx", route_record.expiry);
        ESP_LOGI(TAG,"  Expiry: %4d", route_record.expiry);
        ESP_LOGI(TAG,"  State: %6s", route_state_name[route_record.relay_count]);
        for(int i = 0; i < route_record.relay_count; i++) {
            ESP_LOGI(TAG,"  Path[%d]: 0x%04hx", i, route_record.path[i]);
        }
        ESP_LOGI(TAG," ");
    }
}
void esp_show_route_table()
{
    const char *TAG = "Route Table";
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_info_t route = {};

    ESP_LOGI(TAG, "Zigbee Network Routes:");
    while (ESP_OK == esp_zb_nwk_get_next_route(&itor, &route)) {
        ESP_LOGI(TAG,"Index: %3d", itor);
        ESP_LOGI(TAG, "  DestAddr: 0x%04hx", route.dest_addr);
        ESP_LOGI(TAG, "  NextHop: 0x%04hx", route.next_hop_addr);
        ESP_LOGI(TAG, "  Expiry: %4d", route.expiry);
        ESP_LOGI(TAG, "  State: %6s", route_state_name[route.flags.status]);
        ESP_LOGI(TAG, "  Flags: 0x%02x", *(uint8_t *)&route.flags);
        ESP_LOGI(TAG, "  Group ID: %d", route.flags.group_id);
        ESP_LOGI(TAG, "  Many-to-One: %d", route.flags.many_to_one);
        ESP_LOGI(TAG, "  No Route Cache: %d", route.flags.no_route_cache);
        ESP_LOGI(TAG, "  Route Record Required: %d", route.flags.route_record_required);}
    ESP_LOGI(TAG, " ");

}
void esp_zigbee_include_show_tables(void) 
{
    ESP_LOGI(TAG_include, "Zigbee Network Tables:");
    esp_show_neighbor_table();
    esp_show_route_table();
    esp_show_record_route_table();
}

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};

void button_handler(switch_func_pair_t *button_func_pair)
{
    if(button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        esp_zigbee_include_show_tables();
        //send_topology_report();
    }
}

bool deferred_driver_init(void)
{
    uint8_t button_num = PAIR_SIZE(button_func_pair);
    bool is_initialized = switch_driver_init(button_func_pair, button_num, button_handler);
    return is_initialized;
}

void create_ping_seq(uint16_t dest_addr, uint32_t seq_num)
{
    uint32_t data_length = PAYLOAD_SIZE; // Example payload length
    const char *TAG = "BEACON TASK";
    esp_zb_apsde_data_req_t req = {
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .dst_addr.addr_short = dest_addr,
        .dst_endpoint = 10,                          // Example endpoint
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,      // Example profile ID
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_BASIC,  // Example cluster ID (On/Off cluster)
        .src_endpoint = 10,                          // Example source endpoint
        .asdu_length = data_length,                  // No payload for ping
        .asdu = malloc(data_length * sizeof(uint8_t)), // Allocate memory for ASDU if needed
        .tx_options = ESP_ZB_APSDE_TX_OPT_FRAG_PERMITTED | ESP_ZB_APSDE_TX_OPT_ACK_TX,// Example transmission options
        .use_alias = false,
        .alias_src_addr = 0,
        .alias_seq_num = 0,
        .radius = 4,                                 // Example radius
    };

    ping_payload_t ping_payload;

    if (req.asdu == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ASDU");
        return;
    } 
    esp_fill_random(req.asdu, data_length);

    //Overwriting part of random data with meaningfull data
    ping_payload.seq_num = seq_num;
    ping_payload.send_time = pdTICKS_TO_MS(xTaskGetTickCount());
    ping_payload.max_ping_count = REPEATS;
    uint16_t random_data_offset = 3*sizeof(uint32_t); // Offset to leave space for seq_num, send_time, and max_ping_count
    memcpy(req.asdu, &ping_payload, random_data_offset); // Copy the ping_payload structure into the beginning of the ASDU
    
    ESP_LOGI(TAG, "Sending APS data request to 0x%04hx with %ld bytes, number: %ld", dest_addr, data_length, seq_num);
    while(!esp_zb_lock_acquire(portMAX_DELAY))
    {
        vTaskDelay(0); // Wait before retrying
    };
    ESP_ERROR_CHECK(esp_zb_aps_data_request(&req));
    esp_zb_lock_release();
    free(req.asdu); // Free the allocated memory for ASDU
}


//dziala jako zadanie FreeRTOS - wysyła pingi do koordynatora po wznowieniu zadania
void beacon_task(void *pvParameters)
{
    const char *TAG = "BEACON_TASK";
    uint32_t i = 0;
    data_to_send_t data;
    beacon_task_handle  = xTaskGetCurrentTaskHandle();
    while (1) {
        if(esp_zb_bdb_dev_joined())
        {
            create_ping_seq(DEST_ADDR, i++);
        }
        vTaskDelay(pdMS_TO_TICKS(DELAY_MS)); 
    }
}
