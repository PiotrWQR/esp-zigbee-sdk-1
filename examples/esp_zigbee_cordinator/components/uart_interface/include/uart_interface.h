#define CONFIG_EXAMPLE_UART_BAUD_RATE 115200
#define CONFIG_EXAMPLE_UART_TXD 17
#define CONFIG_EXAMPLE_UART_RXD 16

void uart_interface_init(void);
void rx_task(void *arg);
void tx_task(void *arg);


enum json_information_type_e {
    json_info_type_none = 0,
    json_info_topology = 1,
    json_info_cca = 2,
    json_info_sending_settings = 3,
};

enum request_type_e {
    request_type_none = 0,
    request_type_topology = 1,
    request_type_cca = 2,
    request_type_sending_settings = 3,
    request_type_set_cca = 4,
    request_type_set_sending_settings = 5,
};