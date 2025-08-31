#include <stdio.h>
#include "Helpers.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nwk/esp_zigbee_nwk.h"
#include "switch_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_core.h"
#include "aps/esp_zigbee_aps.h"
#include <memory.h>
#include "esp_err.h"


static const char *TAG_include = "esp_zigbee_include";

static uint32_t byte_counter_out = 0;
static uint32_t byte_counter_in = 0;
static uint32_t byte_count_out = 0;
static uint32_t byte_count_in = 0;
static uint32_t ping_count = 0;
static esp_zb_network_traffic_raport_t traffic_report[3];
//function creating payload and sending it to the destination address
void create_ping(uint16_t dest_addr, bool show_log);
void create_ping_64bit(uint64_t dest_addr);
void create_network_load(uint16_t dest_addr, uint8_t repetitions);
void create_network_load_64bit(uint64_t dest_addr, uint8_t repetitions);



uint16_t request_size(esp_zb_apsde_data_req_t *req) {
    if (!req) {
        return 0;
    }
    uint16_t size = aps_address_modes_size[req->dst_addr_mode];

    size+= 19; // 19 is the size of the fixed fields in esp_zb_apsde_data_req_t
    size += req->asdu_length;
    return size;
}

void traffic_reporter_init(void *pvParameters) {
    byte_counter_out = 0;
    byte_count_out = 0;
    static uint16_t bandwidth = 10;
    while (1) {
        ESP_LOGI(TAG_include, "Byte count in last 10 seconds: %ld", byte_count_out);
        vTaskDelay(pdMS_TO_TICKS(10000)); // Wait for 10 seconds
        byte_count_out = byte_counter_out; // Store the current byte count
        byte_counter_out = 0; // Reset the counter after sending the report;


    }    
}


static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};





esp_zb_apsde_data_req_t create_aps_request(uint16_t dest_addr, uint8_t dst_endpoint, uint8_t src_endpoint,
                                           uint16_t profile_id, uint16_t cluster_id, uint8_t *asdu, uint32_t asdu_length,
                                           uint8_t tx_options, bool use_alias, uint16_t alias_src_addr, int alias_seq_num,
                                           uint8_t radius)
{
    esp_zb_apsde_data_req_t req = {
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .dst_addr.addr_short = dest_addr,
        .dst_endpoint = dst_endpoint,
        .profile_id = profile_id,
        .cluster_id = cluster_id,
        .src_endpoint = src_endpoint,
        .asdu_length = asdu_length,
        .asdu = asdu,
        .tx_options = tx_options,
        .use_alias = use_alias,
        .alias_src_addr = alias_src_addr,
        .alias_seq_num = alias_seq_num,
        .radius = radius
    };
    return req;
}

//wyświetla sąsiadów
static void esp_show_neighbor_table()
{

    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};

    ESP_LOGI(TAG_include,"Network Neighbors:");
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        ESP_LOGI(TAG_include,"Index: %3d", itor);
        ESP_LOGI(TAG_include,"  Age: %3d", neighbor.age);
        ESP_LOGI(TAG_include,"  Neighbor: 0x%04hx", neighbor.short_addr);
        ESP_LOGI(TAG_include,"  IEEE: 0x%016" PRIx64, *(uint64_t *)neighbor.ieee_addr);
        ESP_LOGI(TAG_include,"  Type: %3s", dev_type_name[neighbor.device_type]);
        ESP_LOGI(TAG_include,"  Rel: %c", rel_name[neighbor.relationship]);
        ESP_LOGI(TAG_include,"  Depth: %3d", neighbor.depth);
        ESP_LOGI(TAG_include,"  RSSI: %3d", neighbor.rssi);
        ESP_LOGI(TAG_include,"  LQI: %3d", neighbor.lqi);
        ESP_LOGI(TAG_include,"  Cost: o:%d", neighbor.outgoing_cost);
        ESP_LOGI(TAG_include,"  Timeout coounter: %ld", neighbor.timeout_counter);
        ESP_LOGI(TAG_include,"  Device timeeout: %ld", neighbor.device_timeout);
        ESP_LOGI(TAG_include," ");
    }
}

//wyswietla trasy
static void esp_show_route_table()
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
        ESP_LOGI(TAG_include, "  Flags: 0x%02hx", *(uint8_t *)&route.flags);
        ESP_LOGI(TAG_include," ");
    }
}
void increment_traffice_raport(uint16_t short_addr, esp_zb_network_traffic_raport_t *traffic_raport) {

    for (int i = 0; i < 3; i++) {
        if (traffic_raport[i].short_addr == short_addr) {
            traffic_raport[i].traffic_count++;
            return;
        }
    }

}
static void esp_show_route_record_table()
{
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_record_info_t route = {};

    ESP_LOGI(TAG_include, "Zigbee Network Routes Records:");
    while (ESP_OK == esp_zb_nwk_get_next_route_record(&itor, &route)) {
        ESP_LOGI(TAG_include,"Index: %3d", itor);
        ESP_LOGI(TAG_include, "  DestAddr: 0x%04hx", route.dest_address);
        ESP_LOGI(TAG_include, "  Expiry: %4d", route.expiry);
        ESP_LOGI(TAG_include, "  Relay: %3d", route.relay_count);
        ESP_LOGI(TAG_include, "  Path node 1: %3d", route.path[0]);
        ESP_LOGI(TAG_include, "  Path node 2: %3d", route.path[1]);
        ESP_LOGI(TAG_include, "  Path node 3: %3d", route.path[2]);
        ESP_LOGI(TAG_include, "  Path node 4: %3d", route.path[3]);
        ESP_LOGI(TAG_include, "  Path node 5: %3d", route.path[4]);
        ESP_LOGI(TAG_include, "  Address: %ld", (long)&route);
        ESP_LOGI(TAG_include," ");
    }
}

void esp_zigbee_include_show_tables(void)
{
    ESP_LOGI(TAG_include, "Zigbee Network Tables:");
    esp_show_neighbor_table();
    esp_show_route_table();
    esp_show_route_record_table();
}

void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm)
{
     if (confirm.status == 0x00) {
        ESP_LOGI("APSDE CONFIRM",
                "Sent successfully from endpoint %d, source address 0x%04hx to endpoint %d,"
                "destination address 0x%04hx, tx_time %d ms",
                confirm.src_endpoint, esp_zb_get_short_address(), confirm.dst_endpoint, confirm.dst_addr.addr_short,
            confirm.tx_time);
        // ESP_LOG_BUFFER_CHAR_LEVEL("APSDE CONFIRM", confirm.asdu, confirm.asdu_length, ESP_LOG_INFO);
    } else {
        if(confirm.dst_addr_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT || confirm.dst_addr_mode == ESP_ZB_APS_ADDR_MODE_64_PRESENT_ENDP_NOT_PRESENT) {
            ESP_LOGW("APSDE CONFIRM", "Failed to send APSDE-DATA request to 0x%016" PRIx64 ", error code: %d, tx time %d ms",
                     *(uint64_t *)confirm.dst_addr.addr_long, confirm.status, confirm.tx_time);
        } else if(confirm.dst_addr_mode == ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT || confirm.dst_addr_mode == ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT) {
            ESP_LOGW("APSDE CONFIRM", "Failed to send APSDE-DATA request to 0x%04hx, error code: %d, tx time %d ms",
                     confirm.dst_addr.addr_short, confirm.status, confirm.tx_time);
        }
    }
}

bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind) {
    ESP_LOGI("APSDE INDICATION", "Received APSDE-DATA indication ");
    bool processed = false;
    if(ind.status == 0x00) {
        byte_counter_in += ind.asdu_length + sizeof(esp_zb_apsde_data_ind_t);
        //ESP_LOGI("APSDE bite counter", "Total bytes: %ld", byte_counter_in);
        if(ind.dst_endpoint==20){
            data_recived_t *data = (data_recived_t *)ind.asdu;
            ESP_LOGI("APSDE INDICATION", "Data received from 0x%04hx: start time %ld, end time %ld, duration %ld ms", ind.src_short_addr, data->start_time, data->end_time, data->end_time - data->start_time);
        }
        if(ind.dst_endpoint==10){
            ping_count++;
            ping_payload_t *ping = (ping_payload_t *)ind.asdu;
            uint32_t rtt = esp_log_timestamp() - ping->send_time;
            ESP_LOGI("APSDE INDICATION", "Ping received from 0x%04hx: seq num %ld, send time %ld, rtt %ld ms", ind.src_short_addr, ping->seq_num, ping->send_time, rtt);
        }
        ESP_LOGI("APSDE INDICATION",
                "Received indicator nr %ld from endpoint %d, source address 0x%04hx to endpoint %d,"
                "destination address 0x%04hx, lqi %d, rx_time %d ms, security_status %d",
                ping_count, ind.src_endpoint, ind.src_short_addr, ind.dst_endpoint, ind.dst_short_addr,
                ind.lqi, ind.rx_time, ind.security_status);
                increment_traffice_raport(ind.src_short_addr, traffic_report);
        processed = false;
    } else {
        byte_counter_in += sizeof(esp_zb_apsde_data_ind_t);
        ESP_LOGE("APSDE INDICATION", "Invalid status of APSDE-DATA indication, error code: %d", ind.status);
        processed = false;
    }
    return processed;
}

bool isCoordinator(uint16_t dest_addr) { 
    return (dest_addr == 0x0000);
}

void create_ping_64(uint64_t dest_addr)
{
    uint32_t data_length = 100; // Example payload length
    esp_zb_ieee_addr_t ieee_addr;
    memcpy(ieee_addr, &dest_addr, sizeof(esp_zb_ieee_addr_t)); // Copy the 64-bit address into the ieee_addr variable

    esp_zb_apsde_data_req_t req = {
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
        .dst_endpoint = 27,                                 // Example endpoint
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,              // Example profile ID
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_BASIC,          // Example cluster ID (On/Off cluster)
        .src_endpoint = 10,                                 // Example source endpoint
        .asdu_length = data_length,                         // No payload for ping
        .asdu = malloc(data_length * sizeof(uint8_t)),      // No payload for ping
        .tx_options = 0x04,                                    // Example transmission options
        .use_alias = false,
        .alias_src_addr = 0,
        .alias_seq_num = 0,
        .radius = 3,                                        // Example radius
    };
    memcpy(req.dst_addr.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t)); // Copy the 64-bit address

    for(uint8_t i = 0; i < data_length; i++) {
        req.asdu[i] = i % 256; 
    }

    ESP_LOGI(TAG_include, "Sending APS data request to 0x%016" PRIx64 " with %ld bytes", dest_addr, data_length);
    ESP_LOGI(TAG_include, "Size of request: %d", request_size(&req));
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_aps_data_request(&req);
    esp_zb_lock_release();
    free(req.asdu); // Free the allocated memory for ASDU
}

void create_ping(uint16_t dest_addr, bool show_log)
{
    uint32_t data_length = 50; // Example payload length
    esp_zb_apsde_data_req_t req = {
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .dst_addr.addr_short = dest_addr,
        .dst_endpoint = 10,                          // Example endpoint
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,      // Example profile ID
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_BASIC,  // Example cluster ID (On/Off cluster)
        .src_endpoint = 10,                          // Example source endpoint
        .asdu_length = data_length,                  // No payload for ping
        .asdu = malloc(data_length * sizeof(uint8_t)), // Allocate memory for ASDU if needed
        .tx_options = 0x04,                            // Example transmission options
        .use_alias = false,
        .alias_src_addr = 0,
        .alias_seq_num = 0,
        .radius = 3,                                 // Example radius
    };

    req.dst_endpoint = 27;
    if (req.asdu == NULL) {
        ESP_LOGE(TAG_include, "Failed to allocate memory for ASDU");
        return;
    } else {
        for (uint8_t i = 0; i < data_length; i++) {
            req.asdu[i] = i % 256; // Fill with some data, e.g., incrementing values
        }
    }
    if(show_log){
    ESP_LOGI(TAG_include, "Sending APS data request to 0x%04hx with %ld bytes", dest_addr, data_length);
    }
    if (isCoordinator(dest_addr)) {
        //xQueueAddToSet(apsde_data_requests_queue, &req);
        return;
    }
        

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_aps_data_request(&req);
    esp_zb_lock_release();
    free(req.asdu); // Free the allocated memory for ASDU
}

void zero_traffic_raport(esp_zb_network_traffic_raport_t *traffic_raport)
{
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_info_t route = {};
    
    uint8_t index = 0;


    while (ESP_OK == esp_zb_nwk_get_next_route(&itor, &route)) { 
        traffic_raport[index].short_addr = route.dest_addr;
        traffic_raport[index].traffic_count = 0;
        index++;
    }
}

void display_traffic_report(esp_zb_network_traffic_raport_t *traffic_raport)
{
    ESP_LOGI(TAG_include, "Traffic Report:");
    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG_include, "Device 0x%04hx: %ld packets", traffic_raport[i].short_addr, traffic_raport[i].traffic_count);
    }
}

void button_handler(switch_func_pair_t *button_func_pair)
{
    if(button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        esp_zigbee_include_show_tables();
        //create_ping_64(0x404ccafffe5db4d4); // Example 64-bit address
        //refresh_routes();
        // create_ping_64(0x404ccafffe5de2a8); // Example 64-bit address
        // vTaskDelay(pdMS_TO_TICKS(100));
        // create_ping_64(0x404ccafffe5fa7f4); // Example 64-bit address
        // vTaskDelay(pdMS_TO_TICKS(100));
        // create_ping_64(0x404ccafffe5fb4d4); // Example 64-bit address
        // vTaskDelay(pdMS_TO_TICKS(100));
        ESP_ERROR_CHECK(esp_zb_bdb_open_network(30));
        display_traffic_report(traffic_report);
        zero_traffic_raport(traffic_report);
    }
}

bool deferred_driver_init(void)
{
    uint8_t button_num = PAIR_SIZE(button_func_pair);
    bool is_initialized = switch_driver_init(button_func_pair, button_num, button_handler);
    return is_initialized ;
}

void refresh_routes(void)
{
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_info_t route = {};

    ESP_LOGI(TAG_include, "Refreshing Zigbee Network Routes:");
    while (ESP_OK == esp_zb_nwk_get_next_route(&itor, &route)) {
        create_ping(route.dest_addr, false);
        vTaskDelay(pdMS_TO_TICKS(50)); // Delay to avoid flooding the network
    }
}

void send_traffic_report(void)
{


    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};
    
    const uint8_t traffic_report_endpoint = 70;

    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) { 
    }

}
