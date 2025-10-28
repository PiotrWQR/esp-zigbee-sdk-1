#include "zcl/esp_zigbee_zcl_common.h"
#include "nwk/esp_zigbee_nwk.h"
#include "aps/esp_zigbee_aps.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_type.h"
#include <freertos/queue.h>
#include "cJSON.h"
#define MIN_BACKOFF_EXPONENT           4                                    /* Minimum value of backoff exponent */
#define MAX_BACKOFF_EXPONENT           8                                    /* Maximum value of backoff exponent */
#define MAX_BACKOFF_RETRIES            5                                   /* Maximum number of backoff retries */



void helpers_init(void);

bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind);
void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm);


bool deferred_driver_init();
void esp_zigbee_include_show_tables(void);

cJSON * get_topology_json(void);
void send_traffic_report(void);
void refresh_routes(void);
void traffic_reporter_init(void *pvParameters);
void zero_traffic_raport(void);
void send_settings(uint16_t short_addr);
char *ieee_addr_to_string(esp_zb_ieee_addr_t ieee_addr);
char *ieee_addr_uint64_to_string(uint64_t ieee_addr);
char *short_addr_to_string(uint16_t short_addr);
cJSON * get_transmision_ended_json(void);
void change_delay(uint32_t new_delay_ms);
void change_repeats(uint16_t new_repeats);
void change_dest_addr(uint16_t new_dest_addr);
void change_payload_size(uint16_t new_payload_size);

uint16_t get_repeats(void);
uint16_t get_dest_addr(void);
uint32_t get_delay_ms(void);
uint16_t get_payload_size(void);
void clear_transmision(void);

//tablice pomocnicze
//tablica dopasowania typu urządzenia do nazwy enumeratora
static const char *dev_type_name[] = {
    [ESP_ZB_DEVICE_TYPE_COORDINATOR] = "ZC",
    [ESP_ZB_DEVICE_TYPE_ROUTER]      = "ZR",
    [ESP_ZB_DEVICE_TYPE_ED]          = "ZED",
    [ESP_ZB_DEVICE_TYPE_NONE]        = "UNK",
};
//tablica nazw relacji z innymi urządzeniami
static const char *rel_name[] = {
    [ESP_ZB_NWK_RELATIONSHIP_PARENT]                = "Parent", /* Parent */
    [ESP_ZB_NWK_RELATIONSHIP_CHILD]                 = "Child", /* Child */
    [ESP_ZB_NWK_RELATIONSHIP_SIBLING]               = "Sibling", /* Sibling */
    [ESP_ZB_NWK_RELATIONSHIP_NONE_OF_THE_ABOVE]     = "Others", /* Others */
    [ESP_ZB_NWK_RELATIONSHIP_PREVIOUS_CHILD]        = "Previous Child", /* Previous Child */
    [ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD] = "Unauthenticated Child", /* Unauthenticated Child */
};
//tablica nazw stanów trasy
static const char *route_state_name[] = {
    [ESP_ZB_NWK_ROUTE_STATE_ACTIVE] = "Active",
    [ESP_ZB_NWK_ROUTE_STATE_DISCOVERY_UNDERWAY] = "Disc",
    [ESP_ZB_NWK_ROUTE_STATE_DISCOVERY_FAILED] = "Fail",
    [ESP_ZB_NWK_ROUTE_STATE_INACTIVE] = "Inactive",
};

static const char *rx_to_name[] = {
   [0] = "Reciver is off",
   [1]  = "Reciver is on",
   [2]  = "Reciver is unknown",
};

static const char * nwk_ind_name[] = {
    [ESP_ZB_NWK_COMMAND_STATUS_NO_ROUTE_AVAILABLE]          = "No route available",
    [ESP_ZB_NWK_COMMAND_STATUS_TREE_LINK_FAILURE]           = "Tree link failure",
    [ESP_ZB_NWK_COMMAND_STATUS_NONE_TREE_LINK_FAILURE]      = "None-tree link failure",
    [ESP_ZB_NWK_COMMAND_STATUS_LOW_BATTERY_LEVEL]           = "Low battery level",
    [ESP_ZB_NWK_COMMAND_STATUS_NO_ROUTING_CAPACITY]         = "No routing capacity",
    [ESP_ZB_NWK_COMMAND_STATUS_NO_INDIRECT_CAPACITY]        = "No indirect capacity",
    [ESP_ZB_NWK_COMMAND_STATUS_INDIRECT_TRANSACTION_EXPIRY] = "Indirect transaction expiry",
    [ESP_ZB_NWK_COMMAND_STATUS_TARGET_DEVICE_UNAVAILABLE]   = "Target device unavailable",
    [ESP_ZB_NWK_COMMAND_STATUS_TARGET_ADDRESS_UNALLOCATED]  = "Target address unallocated",
    [ESP_ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE]         = "Parent link failure",
    [ESP_ZB_NWK_COMMAND_STATUS_VALIDATE_ROUTE]              = "Validate route",
    [ESP_ZB_NWK_COMMAND_STATUS_SOURCE_ROUTE_FAILURE]        = "Source route failure",
    [ESP_ZB_NWK_COMMAND_STATUS_MANY_TO_ONE_ROUTE_FAILURE]   = "Many-to-one route failure",
    [ESP_ZB_NWK_COMMAND_STATUS_ADDRESS_CONFLICT]            = "Address conflict",
    [ESP_ZB_NWK_COMMAND_STATUS_VERIFY_ADDRESS]              = "Verify address",
    [ESP_ZB_NWK_COMMAND_STATUS_PAN_IDENTIFIER_UPDATE]       = "Pan ID update",
    [ESP_ZB_NWK_COMMAND_STATUS_NETWORK_ADDRESS_UPDATE]      = "Network address update",
    [ESP_ZB_NWK_COMMAND_STATUS_BAD_FRAME_COUNTER]           = "Bad frame counter",
    [ESP_ZB_NWK_COMMAND_STATUS_BAD_KEY_SEQUENCE_NUMBER]     = "Bad key sequence number", 
    [ESP_ZB_NWK_COMMAND_STATUS_UNKNOWN_COMMAND]             = "Command received is not known",
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

//informacje od urządzenia końcowego  o statystykach zebranych z zakończonej transmisji
typedef struct data_recived_s {
    uint32_t start_time;
    uint32_t end_time;
    uint32_t failed_ping_count;
    uint32_t successful_ping_count;
    esp_zb_ieee_addr_t addr;
    uint32_t recon_time; //czas ponownej konfiguracji sieci po utracie połączenia (jeśli dotyczy)
    uint32_t repeats;
    uint32_t delay;
    uint32_t size;
} data_recived_t;

//ładunek pakietu ping
typedef struct ping_payload_s {
    uint32_t seq_num; //numer nadany indykatorowi przez nadawcę
    uint32_t send_time; // czas wysłania indykatora z nadawcy
    uint32_t max_ping_count; // maksymalna liczba indykatorów do wysłania
    uint8_t *payload; //dodatkowy ładunek(zwykle losowe/ustawione bajty)
} ping_payload_t;

typedef struct {
    uint16_t new_repeats;
    uint16_t new_dest_addr;
    uint32_t new_delay_ms;      // przerwa miedzy żądaniami dla wysyłania sekwancji pingow przez urządzenie końcowe
    uint32_t new_delay_tick; // przerwa miedzy żądaniami dla wysyłania sekwancji pingow przez urządzenie końcowe(nieuzywane)
    uint8_t csma_min_be;        /*!< The minimum value of the backoff exponent, BE, in the CSMA-CA algorithm. */
    uint8_t csma_max_be;        /*!< The maximum value of the backoff exponent, BE, in the CSMA-CA algorithm. */
    uint8_t csma_max_backoffs;  /*!< The maximum number of backoff attempts, NB, in the CSMA-CA algorithm. */
    uint16_t payload;
    int8_t tx_power;
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
// struktura zawierająva dane o sąsiadach i trasach zebrane z urządzenia nadjacego(zazwyczaj routera)
typedef struct topology_report_s {
    esp_zb_ieee_addr_t ieee_addr;
    int16_t neighbor_count;
    neighbor_info_t neighbors[10];
    int16_t routes_count;
    route_info_t routes[10];
} topology_report_t;

//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------




//tablica rozmiarów nagłówków w zależności od trybu adresowania(niedopracowana)
static const uint8_t aps_address_modes_size[] = {
    [ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT]   = 0,
    [ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT]   = 2,
    [ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT]             = 37,
    [ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT]             = 49,
    [ESP_ZB_APS_ADDR_MODE_64_PRESENT_ENDP_NOT_PRESENT] = 8,
};