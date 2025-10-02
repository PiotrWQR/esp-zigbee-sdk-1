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
char* create_json_topology();
char* create_json_cca();
char* create_json_sending_settings();
void realize_host_request(cJSON *json);

// Setup UART buffered IO with event queue
static const int  uart_num = UART_NUM_1;
static const int RX_BUF_SIZE = 1024;
static const int TX_BUF_SIZE = 1024*2;
const int uart_buffer_size = (1024 * 2);
QueueHandle_t uart_queue;
QueueHandle_t uart_tx_queue;




#define TXD_PIN (CONFIG_EXAMPLE_UART_TXD)
#define RXD_PIN (CONFIG_EXAMPLE_UART_RXD)

void init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(uart_num, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, 0, &uart_queue, 0);
    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_tx_queue = xQueueCreate(10, sizeof(char*));
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(uart_num, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

void tx_task(void *arg)
{
    //uart_event_t event;
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    const char* data;
    while (1) {
        if(xQueueReceive(uart_tx_queue, (void * )&data, (TickType_t)portMAX_DELAY)) {
            ESP_LOGI(TX_TASK_TAG, "Received data: %s", data);
            sendData(TX_TASK_TAG, data);
            free(data);
        }
    }
}

void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(uart_num, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            cJSON *json = cJSON_Parse((char *)data);
            cJSON_GetNumberValue(cJSON_GetObjectItem(json, "information_type"));


            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}

char* create_json_topology()
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(root, "information_type", json_info_topology);
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
        char dest_addr_str[8];
        sprintf(dest_addr_str, "0x%04hx", route.dest_addr);
        cJSON_AddStringToObject(route_item, "dest_addr", dest_addr_str);
        cJSON_AddNumberToObject(route_item, "next_hop", route.next_hop_addr);
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

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(dest_addr_str);
    return json_string;
}

void realize_host_request(cJSON *json){
    //TODO mutex for settings change
    uint8_t request_type = cJSON_GetObjectItem(json, "request_type")->valueint;
    switch (request_type)
    {
    case request_type_set_sending_settings:
        if(cJSON_GetObjectItem(json, "repeats") != NULL){
            repeats = cJSON_GetObjectItem(json, "repeats")->valueint;
        }
        if(cJSON_GetObjectItem(json, "dest_addr") != NULL){
            dest_addr = cJSON_GetObjectItem(json, "dest_addr")->valueint;
        }
        if(cJSON_GetObjectItem(json, "delay_ms") != NULL){
            delay_ms = cJSON_GetObjectItem(json, "delay_ms")->valueint;
        }
        send_settings(0xffff); //Send settings to all devices
        break;
    case request_type_set_cca:
        {
            esp_zb_platform_mac_config_t mac_config = {0};
            esp_zb_platform_mac_config_get(&mac_config);
            if(cJSON_GetObjectItem(json, "csma_min_be") != NULL){
                mac_config.csma_min_be = cJSON_GetObjectItem(json, "csma_min_be")->valueint;
            }
            if(cJSON_GetObjectItem(json, "csma_max_be") != NULL){
                mac_config.csma_max_be = cJSON_GetObjectItem(json, "csma_max_be")->valueint;
            }
            if(cJSON_GetObjectItem(json, "csma_max_backoffs") != NULL){
                mac_config.csma_max_backoffs = cJSON_GetObjectItem(json, "csma_max_backoffs")->valueint;
            }
            ESP_ERROR_CHECK(esp_zb_platform_mac_config_set(&mac_config));
            send_settings(0xffff); //Send settings to all devices
        }
    case request_type_topology:
        {
            char* json_string = create_json_topology();
            if(json_string != NULL){
                char* json_copy = strdup(json_string);//Czy kopiowanie jest konieczne?
                if(xQueueSend(uart_tx_queue, &json_copy, (TickType_t)0) != pdTRUE) {
                    free(json_copy);
                }
                free(json_string);
            }
        }
        break;
    case request_type_cca:
        {
            char* json_string = create_json_cca();
            if(json_string != NULL){
                char* json_copy = strdup(json_string);//Czy kopiowanie jest konieczne?
                if(xQueueSend(uart_tx_queue, &json_copy, (TickType_t)0) != pdTRUE) {
                    free(json_copy);
                }
                free(json_string);
            }
        }
        break;
    case request_type_sending_settings:
        {
            char* json_string = create_json_sending_settings();
            if(json_string != NULL){
                char* json_copy = strdup(json_string);//Czy kopiowanie jest konieczne?
                if(xQueueSend(uart_tx_queue, &json_copy, (TickType_t)0) != pdTRUE) {
                    free(json_copy);
                }
                free(json_string);
            }
        }
        break;
    default:
        break;
    }

}
