#include <stdio.h>
#include "uart_interface.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
// Setup UART buffered IO with event queue
const int uart_buffer_size = (1024 * 2);
QueueHandle_t uart0_queue;

Queue

// Install UART driver using an event queue here
void setup_uart() {
    uart_event_t event;
    const int uart_num = UART_NUM_0;
    size_t buffered_size ;
    uint8_t* dtmp = (uint8_t*) malloc(uart_buffer_size);
    assert(dtmp);

    for(;;) {
        // Waiting for UART event.
        if(xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, uart_buffer_size);
            ESP_LOGI("UART", "uart[%d] event:", uart_num);
            switch(event.type) {
                //Event of UART receving data
                case UART_DATA:
                    ESP_LOGI("UART", "[UART DATA]: %d", event.size);
                    uart_read_bytes(uart_num, dtmp, event.size, portMAX_DELAY);
                    ESP_LOGI("UART", "[DATA]: %s", dtmp);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI("UART", "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(uart_num);
                    xQueueReset(uart0_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI("UART", "ring buffer full");
                    // If buffer full happened, you should consider increasing your buffer size
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(uart_num);
                    xQueueReset(uart0_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI("UART", "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI("UART", "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI("UART", "uart frame error");
                    break;
                //Others
                default:
                    ESP_LOGI("UART", "uart event type: %d", event.type);
                    break;
            }
        }
    }
}

