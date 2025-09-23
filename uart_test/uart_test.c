#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define BUFFER_SIZE 256

int main()
{
    stdio_init_all();

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    char json_buffer[BUFFER_SIZE];
    int index = 0;
    char buffer[64];
    int counter = 1;

    absolute_time_t last_send = get_absolute_time();

    while (true)
    {

        if (absolute_time_diff_us(last_send, get_absolute_time()) >= 1000000)
        {
            snprintf(buffer, sizeof(buffer), "{\"counter\": %d}\n", counter);
            uart_puts(UART_ID, buffer);
            counter++;
            last_send = get_absolute_time();
        }

        while (uart_is_readable(UART_ID))
        {
            char c = uart_getc(UART_ID);

            if (index < BUFFER_SIZE - 1)
            {
                json_buffer[index++] = c;
            }

            if (c == '}')
            {
                json_buffer[index] = '\0';
                printf("Trama completa recibida: %s\n", json_buffer);
                index = 0;
            }
        }

        sleep_ms(1000);
    }
}
