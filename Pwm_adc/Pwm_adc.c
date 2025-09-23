#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

int main()
{
    stdio_init_all();
    adc_init();

    adc_select_input(0);

    while (true)
    {
        uint16_t raw = adc_read();
        float voltage = raw * 3.3f / 4095.0f;
        printf("ADC raw: %u, Voltage: %.2f V\n", raw, voltage);
        sleep_ms(300);
    }
}
