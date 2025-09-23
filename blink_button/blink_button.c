#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#define LED_GPIO 8
#define BUTTON_GPIO 7
#define DEBOUNCE_MS 50

void setupGPIO()
{
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);

    gpio_init(BUTTON_GPIO);
    gpio_set_dir(BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(BUTTON_GPIO);
}

int main()
{
    stdio_init_all();
    setupGPIO();

    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    bool last_button_state = true;
    bool led_state = false;

    while (true)
    {
        bool current_button_state = gpio_get(BUTTON_GPIO);

        if (current_button_state != last_button_state)
        {
            sleep_ms(DEBOUNCE_MS);
            current_button_state = gpio_get(BUTTON_GPIO);
            if (current_button_state != last_button_state)
            {

                led_state = !current_button_state;
                gpio_put(LED_GPIO, led_state);
                printf("LED %s\n", led_state ? "ON" : "OFF");
            }
        }

        last_button_state = current_button_state;
        sleep_ms(10);
    }
}
