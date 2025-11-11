#include <stdio.h>
#include <string.h>
#include "uart_interface.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "driver/gpio.h"
#include "cJSON.h"
#include "esp_zigbee_core.h"
#include "Helpers.h"

//Function prototypes
char* create_json_tables();
char* create_json_cca();
char* create_json_sending_settings();
void execute_host_request(cJSON *json);
// Setup UART buffered IO with event queue
static const int  uart_num = UART_NUM_1;
static const int RX_BUF_SIZE = 512;
static const int TX_BUF_SIZE = 1824*2;


void uart_interface_init(void)
{
    ESP_LOGI("uart_interface", "Initializing UART");
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI("uart_interface", "UART init with TXD pin: %d, RXD pin: %d, baud rate: %d", TXD_PIN, RXD_PIN, uart_config.baud_rate);
    ESP_ERROR_CHECK(uart_driver_install(uart_num, RX_BUF_SIZE , TX_BUF_SIZE * 2, 10, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    uart_set_pin(uart_num, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI("uart_interface", "UART initialized");
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    char *data_with_newline = (char *)malloc(len + 2); // +1 for newline, +1 for null terminator
    if (data_with_newline == NULL) {
        ESP_LOGE(logName, "Failed to allocate memory for data_with_newline");
        return -1; // Indicate error
    }
    strcpy(data_with_newline, data);
    sprintf(data_with_newline + len, "\n"); // Append newline character
    int txBytes = uart_write_bytes(uart_num, data_with_newline, len + 1);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    free(data_with_newline);
    return txBytes;
}

void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        int rxBytes = uart_read_bytes(uart_num, data, RX_BUF_SIZE, 50 / portTICK_PERIOD_MS);
        int request_type ;
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Request string: %s", (char *)data);
            cJSON *json = cJSON_Parse((char *)data);
            if(json != NULL){
                if(cJSON_HasObjectItem(json, "request_type")){
                    request_type = cJSON_GetObjectItem(json, "request_type")->valueint;
                    execute_host_request(json);
                    cJSON_Delete(json);
                    continue;
                    
                }else{
                    ESP_LOGW(RX_TASK_TAG, "No request_type in JSON");
                    char* json_string = create_json_error("No request_type in JSON");
                    if(json_string != NULL){
                        sendData(RX_TASK_TAG, json_string);
                        cJSON_Delete(json);
                        free(json_string);
                        continue;
                    }
                }
            } else {
                ESP_LOGW(RX_TASK_TAG, "Received invalid JSON");
                char* json_string = create_json_error("Json is not valid");
                if(json_string != NULL){
                    sendData(RX_TASK_TAG, json_string);
                    free(json_string);
                    continue;
                }
            }
            ESP_LOGI(RX_TASK_TAG, "Request type: %d", request_type);
        }
    }
    free(data);
}


char* create_json_tables()
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(root, "information_type", json_info_tables);
    cJSON_AddArrayToObject(root, "neighbors");  
    cJSON_AddArrayToObject(root, "routes");
    cJSON_AddArrayToObject(root, "route_records");

    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};
    cJSON *neighbors = cJSON_GetObjectItem(root, "neighbors");
    cJSON *routes = cJSON_GetObjectItem(root, "routes");
    cJSON *routes_records = cJSON_GetObjectItem(root, "route_records");
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        cJSON *neighbor_item = cJSON_CreateObject();
        char dest_addr_str[8];
        sprintf(dest_addr_str, "0x%04hx", neighbor.short_addr);
        char long_addr_str[20];
        sprintf(long_addr_str, "0x%016" PRIx64, *(uint64_t *)neighbor.ieee_addr);
        cJSON_AddStringToObject(neighbor_item, "ieee_addr", long_addr_str);
        cJSON_AddStringToObject(neighbor_item, "short_addr", dest_addr_str);
        //cJSON_AddNumberToObject(neighbor_item, "device_type", neighbor.device_type);
        //cJSON_AddNumberToObject(neighbor_item, "relationship", neighbor.relationship);
        cJSON_AddNumberToObject(neighbor_item, "depth", neighbor.depth);
        cJSON_AddNumberToObject(neighbor_item, "lqi", neighbor.lqi);
        cJSON_AddNumberToObject(neighbor_item, "outgoing_cost", neighbor.outgoing_cost);
        cJSON_AddNumberToObject(neighbor_item, "rssi", neighbor.rssi);
        cJSON_AddItemToArray(neighbors, neighbor_item);
    }

    itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_info_t route = {};
    while (ESP_OK == esp_zb_nwk_get_next_route(&itor, &route)) {
        cJSON *route_item = cJSON_CreateObject();
        cJSON_AddStringToObject(route_item, "dest_addr", short_addr_to_string(route.dest_addr));
        cJSON_AddStringToObject(route_item, "next_hop", short_addr_to_string(route.next_hop_addr));
        cJSON_AddNumberToObject(route_item, "flags", *(uint8_t *)&route.flags);
        cJSON_AddItemToArray(routes, route_item);
    }
    itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_record_info_t route_record = {};
    while (ESP_OK == esp_zb_nwk_get_next_route_record(&itor, &route_record)) {
        cJSON *route_record_item = cJSON_CreateObject();
        cJSON_AddStringToObject(route_record_item, "dest_addr", short_addr_to_string(route_record.dest_address));
        cJSON_AddNumberToObject(route_record_item, "expiry", route_record.expiry);
        cJSON *path_array = cJSON_AddArrayToObject(route_record_item, "path");
        for (uint8_t i = 0; i < route_record.relay_count; i++) {
            cJSON_AddItemToArray(path_array, cJSON_CreateString(short_addr_to_string(route_record.path[i])));
        }
        cJSON_AddItemToArray(routes_records, route_record_item);
    }

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

char* create_json_cca()
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }
    cJSON_AddNumberToObject(root, "information_type", json_info_cca);
    esp_zb_platform_mac_config_t mac_config = {0};
    esp_zb_platform_mac_config_get(&mac_config);
    cJSON_AddNumberToObject(root, "csma_min_be", mac_config.csma_min_be);
    cJSON_AddNumberToObject(root, "csma_max_be", mac_config.csma_max_be);
    cJSON_AddNumberToObject(root, "csma_max_backoffs", mac_config.csma_max_backoffs);
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

char* create_json_error(char *error_description)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    if(error_description == NULL)
        error_description = "Unknown error";
    cJSON_AddStringToObject(root, "error_description", error_description);

    cJSON_AddNumberToObject(root, "information_type", json_info_error);
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    printf("Error JSON: %s\n", json_string);
    return json_string;
}

char* create_json_sending_settings()
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(root, "information_type", json_info_sending_settings);
    cJSON_AddNumberToObject(root, "repeats", get_repeats());
    char* dest_addr_str = (char*)malloc(8);
    sprintf(dest_addr_str, "0x%04hx", get_dest_addr());
    cJSON_AddStringToObject(root, "dest_addr_str", dest_addr_str);
    cJSON_AddNumberToObject(root, "dest_addr", get_dest_addr());
    cJSON_AddNumberToObject(root, "delay_ms", get_delay_ms());
    cJSON_AddNumberToObject(root, "payload_size", get_payload_size());
    int8_t tx_power = 0;
    esp_zb_get_tx_power(&tx_power);
    cJSON_AddNumberToObject(root, "tx_power", tx_power);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(dest_addr_str);
    return json_string;
}

char* create_json_topology(){
    cJSON *root =  get_topology_json();
    cJSON_AddNumberToObject(root, "information_type", json_info_topology);
    if (root == NULL) {
        return NULL;
    }
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_string;
}

char* create_json_transmision_ended(){
    cJSON *root =  get_transmision_ended_json();
    cJSON_AddNumberToObject(root, "information_type", json_info_trasmision_ended);

    if (root == NULL) {
        return NULL;
    }

    char *json_string = cJSON_PrintUnformatted(root);
    printf("%s", json_string);
    printf("\n");
    cJSON_Delete(root);

    return json_string;
}

void update_notify_callback(const esp_zb_zdo_mgmt_update_notify_t *notify, void *user_ctx)
{
    const char *TAG = "ZDO MGMT NWK UPDATE NOTIFY CALLBACK";
    ESP_LOGI(TAG, "Notify callback started");
    cJSON * notify_json = cJSON_CreateObject();
    if (notify->status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        cJSON *arr = cJSON_CreateArray();
        cJSON_AddItemToObject(notify_json, "energy_values", arr);
        ESP_LOGI(TAG, "Network Update Notify received successfully");
        for(uint8_t i = 0; i <= 26; i++) {
            if(notify->scanned_channels & (1 << i)) {
                cJSON * channel_item = cJSON_CreateObject();
                ESP_LOGI(TAG, "Channel %d: Energy %d dBm", i, notify->energy_values[i]);
                cJSON_AddItemToObject(channel_item, "energy_value", cJSON_CreateNumber(notify->energy_values[i]));
                cJSON_AddItemToObject(channel_item, "channel", cJSON_CreateNumber(i));
                cJSON_AddItemToArray(arr, channel_item);
            }
        }
        // ESP_LOGI(TAG, "Total Transmissions: %d", notify->total_transmission);
        // ESP_LOGI(TAG, "Transmission Failures: %d", notify->transmission_failures);
        cJSON_AddNumberToObject(notify_json, "total_transmission", notify->total_transmission);
        cJSON_AddNumberToObject(notify_json, "transmission_failures", notify->transmission_failures);
    } else {
        cJSON_AddNumberToObject(notify_json, "status", notify->status);
        ESP_LOGE(TAG, "Network Update Notify failed with status: 0x%02x", notify->status);
    }
    cJSON_AddNumberToObject(notify_json, "information_type", json_info_energy_scan);
    char * result = cJSON_PrintUnformatted(notify_json);
    cJSON_Delete(notify_json);
    sendData(TAG, result);
    free(result);
    
}

char * create_json_nwk(){
    cJSON *root = cJSON_CreateObject();
    char* json_string = NULL;
    if (root == NULL) {
        json_string = create_json_error("Failed to create JSON object");
        return json_string;
    }
    esp_zb_ieee_addr_t extended_pan_id;
    esp_zb_get_extended_pan_id(extended_pan_id);
    cJSON_AddNumberToObject(root, "information_type", json_info_nwk_data);
    cJSON_AddStringToObject(root, "extended_pan_id", ieee_addr_to_string(extended_pan_id));
    cJSON_AddStringToObject(root, "pan_id", short_addr_to_string(esp_zb_get_pan_id()));
    cJSON_AddNumberToObject(root, "channel", esp_zb_get_current_channel());



    json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

void execute_host_request(cJSON *json){
    //TODO mutex for settings change
    uint8_t request_type = cJSON_GetObjectItem(json, "request_type")->valueint;
    const char TAG[] = "REQUEST HANDLER";
    
    switch (request_type){
    case request_type_set_sending_settings:
        {
            if(cJSON_HasObjectItem(json, "device_addr")){
                uint16_t device_addr = cJSON_GetObjectItem(json, "device_addr")->valueint;
                esp_zb_set_tx_power(0); //Reset tx power to default before sending new settings
                setting_change_t settings = {0};
                settings.new_dest_addr = get_dest_addr();
                settings.new_delay_ms = get_delay_ms();
                
                if(cJSON_HasObjectItem(json, "dest_addr")){
                    settings.new_dest_addr = cJSON_GetObjectItem(json, "dest_addr")->valueint;
                }
                if(cJSON_HasObjectItem(json, "delay_ms") ){
                    uint16_t delay_ms = cJSON_GetObjectItem(json, "delay_ms")->valueint;
                    settings.new_delay_ms = delay_ms;
                    ESP_LOGI(TAG, "Delay set to %ld ms", get_delay_ms());
                }
                if(cJSON_HasObjectItem(json, "payload_size")){
                    settings.payload_size = cJSON_GetObjectItem(json, "payload_size")->valueint;
                    if(settings.payload_size > 1600){
                        settings.payload_size = 1600;
                        ESP_LOGI(TAG, "Payload size too large, set to max 1600");  
                    }
                }
                if(cJSON_HasObjectItem(json, "tx_power"))
                {
                    int8_t tx_power = cJSON_GetObjectItem(json, "tx_power")->valueint;
                    settings.tx_power = tx_power;
                }
                send_settings(device_addr, &settings); //Send settings to specific device
                ESP_LOGI(TAG, "Tu działa");
            }
        } 
        break;
    case request_type_set_cca:
        {
            ESP_LOGE(TAG, "Deprecated: CCA settings change requested");
            esp_zb_platform_mac_config_t mac_config = {0};
            int8_t changed = 0;
            if(cJSON_HasObjectItem(json, "csma_min_be")){
                mac_config.csma_min_be = cJSON_GetObjectItem(json, "csma_min_be")->valueint;
                changed = 1;
            }
            if(cJSON_HasObjectItem(json, "csma_max_be")){
                mac_config.csma_max_be = cJSON_GetObjectItem(json, "csma_max_be")->valueint;
                changed = 1;
            }
            if(cJSON_HasObjectItem(json, "csma_max_backoffs")){
                mac_config.csma_max_backoffs = cJSON_GetObjectItem(json, "csma_max_backoffs")->valueint;
                changed = 1;
            }

            esp_zb_platform_mac_config_set(&mac_config);
            if(changed){
                ESP_LOGI(TAG, "MAC config updated");
                char* json_string = create_json_cca();
                if(json_string != NULL){
                    sendData(TAG, json_string);
                    free(json_string);
                }
            }
            else{
                ESP_LOGI(TAG, "Failed to set MAC config");
                char* json_string = create_json_error("Failed to set MAC config, one or more parameters are invalid");
                if(json_string != NULL){
                    sendData(TAG, json_string);
                    free(json_string);  
                }
        }}
        break;
    case request_type_tables:
        {
            char* json_string = create_json_tables();
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            }
        }
        break;
    case request_type_cca:
        {
            char* json_string = create_json_cca();
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            }
        }
        break;
    case request_type_sending_settings:
        {
            char* json_string = create_json_sending_settings();
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            }
        }
        break;
    case request_type_topology:
        {
            char* json_string = create_json_topology();
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            } else {
                ESP_LOGI(TAG, "Topology is empty");
                char* json_string = create_json_error("Topology is empty");
                if(json_string != NULL){
                    sendData(TAG, json_string);
                    free(json_string);
                }
            }
        }
        break;
    case request_type_nwk_data:
        {
            char * json_string = create_json_nwk();
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            }
        }
        break;
    case request_type_trasmision_ended:
        {
            char* json_string = create_json_transmision_ended();
            ESP_LOGI(TAG, "%s", json_string);
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            } 
        }
        break;
    case request_type_clear_transmission:
        {
            clear_transmision();
        }
        break;
    case request_type_open_network:
    {
        esp_zb_bdb_open_network(30);
    }
        break;
    case request_type_reset_network:
    {
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
    }
    break;
    case request_send_all_data:
        {
            send_all_data_to_host(TAG);
        }
        break;
    default:
        {
            ESP_LOGI(TAG, "Unknown request type: %d", request_type);
            char fstring[50];
            sprintf(fstring, "Not recognized request type: %d", request_type);
            char* json_string = create_json_error(fstring);
            free(fstring);
            if(json_string != NULL){
                sendData(TAG, json_string);
            }
        }
        break;
    }

}

void send_all_data_to_host(const char TAG[]){
    char* json_string = create_json_cca();
    if(json_string != NULL){
        sendData(TAG, json_string);
        free(json_string);
    }
    json_string = create_json_sending_settings();
    if(json_string != NULL){
        sendData(TAG, json_string);
        free(json_string);
    }
    json_string = create_json_tables();
    if(json_string != NULL){
        sendData(TAG, json_string);
        free(json_string);
    }
    json_string = create_json_topology();
    if(json_string != NULL){
        sendData(TAG, json_string);
        free(json_string);
    }
    json_string = create_json_nwk();
    if(json_string != NULL){
        sendData(TAG, json_string);
        free(json_string);
    }
    json_string = create_json_transmision_ended();
    if(json_string != NULL){
        sendData(TAG, json_string);
        free(json_string);
    }
    const esp_zb_zdo_mgmt_nwk_update_req_param_t update_req = {
        .scan_channels = 0x07FFF800, // Example channel mask
        .scan_duration = 3,             // Example scan duration
    };
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zdo_mgmt_nwk_update_req(&update_req, update_notify_callback, NULL);
    esp_zb_lock_release();
}

void send_ping_data(esp_zb_apsde_data_ind_t * ind, uint32_t ping_num, uint32_t seq_num){
    //ESP_LOGI("PING DATA", "Address mode: %s", addr_mode_name[ind->dst_addr_mode]);
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "seq_num", cJSON_CreateNumber(seq_num));
    cJSON_AddItemToObject(obj, "ping_num", cJSON_CreateNumber(ping_num));
    cJSON_AddItemToObject(obj, "addr", cJSON_CreateNumber(ind->src_short_addr));
    cJSON_AddItemToObject(obj, "lqi", cJSON_CreateNumber(ind->lqi));
    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_record_info_t route_record = {};
    cJSON * path = cJSON_AddArrayToObject(obj, "path");
    while (ESP_OK == esp_zb_nwk_get_next_route_record(&itor, &route_record))
    {
        if(route_record.dest_address == ind->dst_short_addr){
            for(uint8_t i=0; i<route_record.relay_count; i++){
                cJSON_AddItemToArray(path, cJSON_CreateNumber(route_record.path[i]));
            }
        }
    }
    itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_route_info_t route = {};
    while (ESP_OK == esp_zb_nwk_get_next_route(&itor, &route))
    {
        if(route.dest_addr == ind->src_short_addr){
            cJSON_AddNumberToObject(obj, "route", route.next_hop_addr);
        }
    }
    cJSON_AddItemToObject(obj, "information_type", cJSON_CreateNumber(json_info_recived_signal));
    char * json_str = cJSON_PrintUnformatted(obj);
    sendData("Data", json_str);
    free(json_str);
    cJSON_Delete(obj);
}