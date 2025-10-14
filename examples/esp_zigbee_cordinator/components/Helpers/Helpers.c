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
#include "esp_task_wdt.h"
#include "cJSON.h"
static const char *TAG_include = "Helpers";

static uint32_t byte_counter_out = 0;
static uint32_t byte_counter_in = 0;
static uint32_t byte_count_out = 0;
static uint32_t byte_count_in = 0;
static uint32_t ping_count = 0;
static esp_zb_network_traffic_raport_t traffic_raport[10];
static cJSON *topology_json = NULL;
static cJSON *transmision_ended_json = NULL;
//function creating payload and sending it to the destination address
void create_ping(uint16_t dest_addr, bool show_log);
void create_ping_64bit(uint64_t dest_addr);
void create_network_load(uint16_t dest_addr, uint8_t repetitions);
void create_network_load_64bit(uint64_t dest_addr, uint8_t repetitions);
void send_indicator_toall(void);

void helpers_init(void) {
    topology_json = cJSON_CreateObject();
    transmision_ended_json = cJSON_CreateObject();
    cJSON * arr = cJSON_CreateArray();
    cJSON_AddItemToObject(transmision_ended_json, "arr", arr);
}

char* ieee_addr_to_string(esp_zb_ieee_addr_t ieee_addr) {
    static char str[24];
    snprintf(str, sizeof(str), "0x%02x%02x%02x%02x%02x%02x%02x%02x",
             ieee_addr[7], ieee_addr[6], ieee_addr[5], ieee_addr[4],
             ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]);
    return str;
}

char* ieee_addr_uint64_to_string(uint64_t ieee_addr) {
    esp_zb_ieee_addr_t arr = {0};
    memcpy(arr, &ieee_addr, sizeof(uint64_t));
    return ieee_addr_to_string(arr);
}

char* short_addr_to_string(uint16_t short_addr) {
    static char str[7];
    snprintf(str, sizeof(str), "0x%04hx", short_addr);
    return str;
}

bool isCoordinator(uint16_t dest_addr) {
    return (dest_addr == 0x0000);
}
//ta funkcja ma wyśetlić ile bajtów zostało wysłanych, jednal istnieje problem z nie zawsze oczywistą wielkością nagłówka oraz stylu fragmentacji
uint16_t request_size(esp_zb_apsde_data_req_t *req) {
    if (!req) {
        return 0;
    }
    uint16_t size = aps_address_modes_size[req->dst_addr_mode];

    size+= 19; // 19 is the size of the fixed fields in esp_zb_apsde_data_req_t
    size += req->asdu_length;
    return size;
}

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};



//Wysłanie ustawień do urządzenia o podanym adresie krótkim - użyte przy potwierdzniu autoryzacji
void send_settings(uint16_t short_addr){
    setting_change_t settings = {
        .new_repeats = repeats,
        .new_dest_addr = dest_addr,
        .new_delay_ms = delay_ms,
        .new_delay_tick = pdMS_TO_TICKS(delay_ms),
        .csma_min_be = MIN_BACKOFF_EXPONENT,
        .csma_max_be = MAX_BACKOFF_EXPONENT,
        .csma_max_backoffs = MAX_BACKOFF_RETRIES,
        .payload = payload_size
    };
    esp_zb_apsde_data_req_t req = {
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .dst_addr.addr_short = short_addr,
        .dst_endpoint = 30,
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        .src_endpoint = 30,
        .asdu_length = sizeof(setting_change_t),
        .asdu = (uint8_t *)&settings,
        .tx_options = 0,
        .use_alias = false,
        .alias_src_addr = 0,
        .alias_seq_num = 0,
        .radius = 2
    };
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_aps_data_request(&req);
    esp_zb_lock_release();
}


//wyświetla sąsiadów w konsoli
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

void increment_traffic_raport(uint16_t short_addr, uint32_t max_ping_count ,uint32_t seq_num) {

    uint8_t i = 0;
    while (traffic_raport[i].is_active && i < 10) {
        if (traffic_raport[i].short_addr == short_addr) {
            traffic_raport[i].traffic_count++;
            while(traffic_raport[i].last_seq_num < seq_num) {
                //ESP_LOGI(TAG_include, "Device 0x%04hx: missed packet %ld", traffic_raport[i].short_addr, traffic_raport[i].last_seq_num);
                // traffic_raport[i].missed_packets++;
                traffic_raport[i].last_seq_num++;
            }

            return;
        }
        i++;
    }
    if(i < 10) {
        traffic_raport[i].short_addr = short_addr;
        traffic_raport[i].traffic_count = 1;
        traffic_raport[i].is_active = true;
        traffic_raport[i].max_ping_count = max_ping_count;
    }
    else {
        traffic_raport[0].short_addr = short_addr;
        traffic_raport[0].traffic_count = 1;
        traffic_raport[0].is_active = true;
        traffic_raport[0].max_ping_count = max_ping_count;
    }

}

int get_traffic_raport_index(esp_zb_network_traffic_raport_t *traffic_raport, uint16_t short_addr)
{
    for (int i = 0; i < 10; i++) {
        if (traffic_raport[i].short_addr == short_addr) {
            return i;
        }
    }
    return -1; // Not found
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
            ESP_LOGW("APSDE INDICATION", "Data received from 0x%04hx: start time %ld, end time %ld, duration %ld ms", ind.src_short_addr, data->start_time, data->end_time, data->end_time - data->start_time);
            cJSON * item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "short_addr", short_addr_to_string(ind.src_short_addr));
            cJSON_AddStringToObject(item, "ieee_addr", ieee_addr_to_string(data->addr));
            cJSON_AddNumberToObject(item, "start_time", data->start_time);
            cJSON_AddNumberToObject(item, "end_time", data->end_time);
            cJSON_AddNumberToObject(item, "duration_ms", data->end_time - data->start_time);
            cJSON_AddNumberToObject(item, "successful_pings", data->successful_ping_count);
            cJSON_AddNumberToObject(item, "failed_pings", data->failed_ping_count);
            cJSON_AddNumberToObject(item, "recon_time", data->recon_time);
            cJSON * arr = cJSON_GetObjectItem(transmision_ended_json,"arr");
            cJSON_AddItemToArray(arr, item);
        }
        if(ind.dst_endpoint==10){
            ping_count++;
            ping_payload_t *ping = (ping_payload_t *)ind.asdu;
            increment_traffic_raport(ind.src_short_addr, ping->max_ping_count, ping->seq_num);
            ESP_LOGI("APSDE INDICATION", "Ping received from 0x%04hx: seq num %ld, send time %ld", ind.src_short_addr, ping->seq_num, ping->send_time);
        }
        if(ind.dst_endpoint==32){
            topology_report_t *topology = (topology_report_t *)ind.asdu;
            ESP_LOGI("APSDE INDICATION TOPOLOGY REPORT", " Topology report received from 0x%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x: neighbor count %d, routes count %d", topology->ieee_addr[7], topology->ieee_addr[6],topology->ieee_addr[5],
            topology->ieee_addr[4],topology->ieee_addr[3],topology->ieee_addr[2],topology->ieee_addr[1],topology->ieee_addr[0]
                , topology->neighbor_count, topology->routes_count);
                cJSON *topology_report = cJSON_CreateObject();
                cJSON_AddNumberToObject(topology_report, "neighbor_count", topology->neighbor_count);
                cJSON_AddNumberToObject(topology_report, "routes_count", topology->routes_count);
                cJSON *neighbors = cJSON_CreateArray();
            for(uint16_t i = 0; i < topology->neighbor_count; i++) {
                neighbor_info_t *neighbor = &topology->neighbors[i];
                cJSON *neighbor_json = cJSON_CreateObject();
                cJSON_AddStringToObject(neighbor_json, "short_addr", short_addr_to_string(neighbor->short_addr));
                cJSON_AddNumberToObject(neighbor_json, "lqi", neighbor->lqi);
                cJSON_AddNumberToObject(neighbor_json, "relationship", neighbor->relationship);
                cJSON_AddNumberToObject(neighbor_json, "device_type", neighbor->device_type);
                cJSON_AddNumberToObject(neighbor_json, "rssi", neighbor->rssi);
                cJSON_AddNumberToObject(neighbor_json, "outgoing_cost", neighbor->outgoing_cost);
                cJSON_AddStringToObject(neighbor_json, "ieee_addr", ieee_addr_uint64_to_string(neighbor->ieee_addr));
                cJSON_AddItemToArray(neighbors, neighbor_json);
                ESP_LOGI("APSDE INDICATION TOPOLOGY REPORT", "Neighbor %d:, short_addr 0x%04hx, lqi %d, relationship %d, device type %d, rssi %d, outgoing cost: %d, ieee addr: 0x%016" PRIx64"", 
                    i, neighbor->short_addr, neighbor->lqi, neighbor->relationship, neighbor->device_type, neighbor->rssi, neighbor->outgoing_cost, neighbor->ieee_addr);
            }
            cJSON_AddItemToObject(topology_report, "neighbors", neighbors);
            cJSON *routes = cJSON_CreateArray();
            for(uint16_t i = 0; i < topology->routes_count; i++) {
                route_info_t *route = &topology->routes[i];
                cJSON *route_json = cJSON_CreateObject();
                cJSON_AddStringToObject(route_json, "dest_addr", short_addr_to_string(route->dest_addr));
                cJSON_AddStringToObject(route_json, "next_hop", short_addr_to_string(route->next_hop));
                cJSON_AddItemToArray(routes, route_json);
                ESP_LOGI("APSDE INDICATION TOPOLOGY REPORT", "Route %d: dest addr 0x%04hx, next hop 0x%04hx", i, route->dest_addr, route->next_hop);
            }
            cJSON_AddItemToObject(topology_report, "routes", routes);
            char *ieee_str = ieee_addr_to_string(topology->ieee_addr);
            printf("IEEE Address: %s\n", ieee_str);
            cJSON_AddItemToObject(topology_json, ieee_str, topology_report);
            char *json_string = cJSON_Print(topology_json);
            if (json_string != NULL) {
                printf("Topology JSON: %s\n", json_string);
                free(json_string);
            } else {
                ESP_LOGE("APSDE INDICATION TOPOLOGY REPORT", "Failed to print JSON");
            }
        }
        ESP_LOGI("APSDE INDICATION",
                "Received indicator nr %ld from endpoint %d, source address 0x%04hx to endpoint %d,"
                "destination address 0x%04hx, lqi %d, rx_time %d ms, security_status %d",
                ping_count, ind.src_endpoint, ind.src_short_addr, ind.dst_endpoint, ind.dst_short_addr,
                ind.lqi, ind.rx_time, ind.security_status);
        processed = false;
    } else {
        ESP_LOGE("APSDE INDICATION", "Invalid status of APSDE-DATA indication, error code: %d", ind.status);
        processed = false;
    }
    return processed;
}


void create_ping(uint16_t dest_addr, bool show_log)
{
    uint32_t data_length = 50; // Example payload length
    esp_zb_apsde_data_req_t req = {
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .dst_addr.addr_short = dest_addr,
        .dst_endpoint = 32,                          // Example endpoint
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,       // Example profile ID
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_BASIC,   // Example cluster ID (On/Off cluster)
        .src_endpoint = 10,                          // Example source endpoint
        .asdu_length = data_length,                  // No payload for ping
        .asdu = malloc(data_length * sizeof(uint8_t)), // Allocate memory for ASDU if needed
        .tx_options = 0x04 | 0x08,                   // Example transmission options
        .use_alias = false,
        .alias_src_addr = 0,
        .alias_seq_num = 0,
        .radius = 3,                                 // Example radius
    };

    //req.dst_endpoint = 27;
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
    if (dest_addr == 0x0000) {
        free(req.asdu); // Free the allocated memory for ASDU
        return;
    }

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_aps_data_request(&req);
    esp_zb_lock_release();
    free(req.asdu); // Free the allocated memory for ASDU
}

void zero_traffic_raport()
{

    for(uint8_t i = 0; i < 10; i++) {
        traffic_raport[i].is_active = false;
        traffic_raport[i].short_addr = 0;
        traffic_raport[i].traffic_count = 0;
    }
}

void display_traffic_report()
{
    ESP_LOGI(TAG_include, "Traffic Report:");
    int i =0;
    while(traffic_raport[i].is_active && i < 10) {
        ESP_LOGI(TAG_include, "Device 0x%04hx: %ld packets received, %ld packets lost, expected: %ld", traffic_raport[i].short_addr, traffic_raport[i].traffic_count , traffic_raport[i].max_ping_count- traffic_raport[i].traffic_count, traffic_raport[i].max_ping_count);
        i++;
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
        send_indicator_toall();
        display_traffic_report();
        zero_traffic_raport();
    }
}

bool deferred_driver_init(void)
{
    uint8_t button_num = PAIR_SIZE(button_func_pair);
    bool is_initialized = switch_driver_init(button_func_pair, button_num, button_handler);
    return is_initialized ;
}


void send_indicator_toall(void)
{
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};

    ESP_LOGI(TAG_include, "Sending indicator to all neighbors:");
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        create_ping(neighbor.short_addr, true);
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay to avoid flooding the network
    }
}

cJSON * get_topology_json(void) {
    send_indicator_toall();
    vTaskDelay(pdMS_TO_TICKS(3000)); // Wait for responses to be received
    cJSON *result = cJSON_Duplicate(topology_json, 1);
    // printf("Topology JSON: %s\n", cJSON_PrintUnformatted(result));
    return result;
}

cJSON * get_transmision_ended_json(void) {
    cJSON *result = cJSON_Duplicate(transmision_ended_json, 1);
    // printf("Transmision Ended JSON: %s\n", cJSON_PrintUnformatted(result));
    return result;
}