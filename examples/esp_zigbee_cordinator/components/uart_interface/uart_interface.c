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

// static const char *TAG = "uart_interface";

//Function prototypes
char* create_json_tables();
char* create_json_cca();
char* create_json_sending_settings();
void execute_host_request(cJSON *json);

// Setup UART buffered IO with event queue
static const int  uart_num = UART_NUM_1;
static const int RX_BUF_SIZE = 512;
static const int TX_BUF_SIZE = 1024*2;
static QueueHandle_t uart_queue;
static cJSON *topology_json = NULL;
//static QueueHandle_t uart_tx_queue;



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
    ESP_ERROR_CHECK(uart_driver_install(uart_num, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, 10, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    uart_set_pin(uart_num, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI("uart_interface", "UART initialized");
}

void update_topology_json(cJSON *topology_report, const char* ieee_str){
    if(topology_json == NULL){
        topology_json = cJSON_CreateObject();
    }
    cJSON_AddItemToObject(topology_json, ieee_str, topology_report);
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
    return txBytes;
}



void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";

    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        int rxBytes = uart_read_bytes(uart_num, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
        int request_type ;
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            cJSON *json = cJSON_Parse((char *)data);
            if(json != NULL){
                if(cJSON_GetObjectItem(json, "request_type") == NULL){
                    ESP_LOGW(RX_TASK_TAG, "No request_type in JSON");
                    char* json_string = create_json_error("No request_type in JSON");
                    if(json_string != NULL){
                        sendData(RX_TASK_TAG, json_string);
                        free(json_string);
                        cJSON_Delete(json);
                        continue;
                    }
                }
                request_type = cJSON_GetObjectItem(json, "request_type")->valueint;
                execute_host_request(json);

                cJSON_Delete(json);
            } else {
                ESP_LOGW(RX_TASK_TAG, "Received invalid JSON");
                char* json_string = create_json_error("Json is not valid");
                if(json_string != NULL){
                    sendData(RX_TASK_TAG, json_string);
                    free(json_string);
                    continue;
                }
            }

            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: %s", rxBytes, data);
            ESP_LOGI(RX_TASK_TAG, "information_type: %d", request_type);
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

    esp_zb_nwk_info_iterator_t itor = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t neighbor = {};
    cJSON *neighbors = cJSON_GetObjectItem(root, "neighbors");
    cJSON *routes = cJSON_GetObjectItem(root, "routes");
    while (ESP_OK == esp_zb_nwk_get_next_neighbor(&itor, &neighbor)) {
        cJSON *neighbor_item = cJSON_CreateObject();
        char dest_addr_str[8];
        sprintf(dest_addr_str, "0x%04hx", neighbor.short_addr);
        char long_addr_str[20];
        sprintf(long_addr_str, "0x%016" PRIx64, *(uint64_t *)neighbor.ieee_addr);
        cJSON_AddStringToObject(neighbor_item, "ieee_addr", long_addr_str);
        cJSON_AddStringToObject(neighbor_item, "short_addr", dest_addr_str);
        cJSON_AddNumberToObject(neighbor_item, "device_type", neighbor.device_type);
        cJSON_AddNumberToObject(neighbor_item, "relationship", neighbor.relationship);
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
    cJSON_AddNumberToObject(root, "repeats", repeats);
    char* dest_addr_str = (char*)malloc(8);
    sprintf(dest_addr_str, "0x%04hx", dest_addr);
    cJSON_AddStringToObject(root, "dest_addr_str", dest_addr_str);
    cJSON_AddNumberToObject(root, "dest_addr", dest_addr);
    cJSON_AddNumberToObject(root, "delay_ms", delay_ms);
    cJSON_AddNumberToObject(root, "payload_size", payload_size);

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
    printf(json_string);
    cJSON_Delete(root);

    return json_string;
}

void execute_host_request(cJSON *json){
    //TODO mutex for settings change
    uint8_t request_type = cJSON_GetObjectItem(json, "request_type")->valueint;
    const char TAG[] = "request_handler";
    switch (request_type){
    case request_type_set_sending_settings:
        {
            if(cJSON_GetObjectItem(json, "repeats") != NULL){
                repeats = cJSON_GetObjectItem(json, "repeats")->valueint;
            }
            if(cJSON_GetObjectItem(json, "dest_addr") != NULL){
                dest_addr = cJSON_GetObjectItem(json, "dest_addr")->valueint;
            }
            if(cJSON_GetObjectItem(json, "delay_ms") != NULL){
                delay_ms = cJSON_GetObjectItem(json, "delay_ms")->valueint;
            }
            if(cJSON_GetObjectItem(json, "payload_size") != NULL){
                payload_size = cJSON_GetObjectItem(json, "payload_size")->valueint;
            }
            send_settings(0xffff);
        } //Send settings to all devices
        break;
    case request_type_set_cca:
        {
            esp_zb_platform_mac_config_t mac_config = {0};
            esp_zb_platform_mac_config_get(&mac_config);
            int8_t changed = 0;
            if(cJSON_GetObjectItem(json, "csma_min_be") != NULL){
                mac_config.csma_min_be = cJSON_GetObjectItem(json, "csma_min_be")->valueint;
                changed = 1;
            }
            if(cJSON_GetObjectItem(json, "csma_max_be") != NULL){
                mac_config.csma_max_be = cJSON_GetObjectItem(json, "csma_max_be")->valueint;
                changed = 1;
            }
            if(cJSON_GetObjectItem(json, "csma_max_backoffs") != NULL){
                mac_config.csma_max_backoffs = cJSON_GetObjectItem(json, "csma_max_backoffs")->valueint;
                changed = 1;
            }
            if(esp_zb_platform_mac_config_set(&mac_config) == ESP_OK && changed) {
                send_settings(0xffff); //Send settings to all devices
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
            cJSON *root = cJSON_CreateObject();
            if (root == NULL) {
                char* json_string = create_json_error("Failed to create JSON object");
                if(json_string != NULL){
                    sendData(TAG, json_string);
                    free(json_string);
                }
                break;
            }
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            cJSON_AddNumberToObject(root, "information_type", json_info_nwk_data);
            cJSON_AddStringToObject(root, "extended_pan_id", ieee_addr_to_string(extended_pan_id));
            cJSON_AddStringToObject(root, "pan_id", short_addr_to_string(esp_zb_get_pan_id()));
            cJSON_AddNumberToObject(root, "channel", esp_zb_get_current_channel());

            char *json_string = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            }
        }
        break;
    case request_type_trasmision_ended:
        {
            char* json_string = create_json_transmision_ended();
            if(json_string != NULL){
                sendData(TAG, json_string);
                free(json_string);
            } 
        }
        break;
    default:
        ESP_LOGI(TAG, "Unknown request type: %d", request_type);
        char fstring[50];
        sprintf(fstring, "Not recognized request type: %d", request_type);
        char* json_string = create_json_error(fstring);
        if(json_string != NULL){
            sendData(TAG, json_string);
            free(json_string);
        }
        free(fstring);
        break;
    }

}
