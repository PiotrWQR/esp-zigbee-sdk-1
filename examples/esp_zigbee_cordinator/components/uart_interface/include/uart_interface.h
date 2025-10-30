
#define CONFIG_EXAMPLE_UART_BAUD_RATE 115200
#define TXD_PIN (GPIO_NUM_6)
#define RXD_PIN (GPIO_NUM_7)

void uart_interface_init(void);
void rx_task(void *arg);
char* create_json_error(char *error_description);
void send_all_data_to_host(const char TAG[]);
void send_ping_data(uint16_t addr, uint32_t ping_num, uint32_t seq_num);

enum json_information_type_e {
    json_info_type_none = 0,
    json_info_topology = 1,
    json_info_cca = 2,
    json_info_sending_settings = 3,
    json_info_tables = 4,
    json_info_nwk_data = 5,
    json_info_trasmision_ended = 6,
    json_info_energy_scan = 7,
    json_info_recived_signal = 8,
    json_info_error = 255,
};

enum request_type_e {
    request_type_none = 0,
    request_type_topology = 1,
    request_type_cca = 2,
    request_type_sending_settings = 3,
    request_type_set_cca = 4,
    request_type_set_sending_settings = 5,
    request_type_tables = 6,
    request_type_nwk_data = 7,
    request_type_trasmision_ended = 8,
    request_type_clear_transmission = 9,
    request_type_open_network = 10,
    request_type_reset_network = 11,
    request_send_all_data = 13,
};