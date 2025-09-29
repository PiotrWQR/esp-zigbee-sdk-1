#define CONFIG_EXAMPLE_UART_BAUD_RATE 115200
#define CONFIG_EXAMPLE_UART_TXD 17
#define CONFIG_EXAMPLE_UART_RXD 16

void init(void);
void rx_task(void *arg);
void tx_task(void *arg);

