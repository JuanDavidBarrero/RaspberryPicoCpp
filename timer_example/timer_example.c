#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"

struct repeating_timer timer;
volatile bool timer_triggered = false;
volatile bool timer_repeat_triggered = false;
uint8_t count = 0;

int64_t alarm_callback(alarm_id_t id, void *user_data)
{
    printf("Timer %d fired!\n", (int)id);
    timer_triggered = true;
    return 0;
}

bool repeating_timer_callback(__unused struct repeating_timer *t)
{
    printf("Repeat at %lld\n", time_us_64());
    timer_repeat_triggered = true;
    return true;
}

int main()
{
    stdio_init_all();

    add_alarm_in_ms(10000, alarm_callback, NULL, true);
    add_repeating_timer_ms(1000, repeating_timer_callback, NULL, &timer);

    sleep_ms(2000);

    printf("===============================\n");
    printf("Hello Timer!\n");
    printf("===============================\n");

    while (true)
    {
        while (!timer_triggered)
        {
            tight_loop_contents();
        }

        if (timer_repeat_triggered)
        {
            count++;
            timer_repeat_triggered = false;
        }

        if (count == 10)
        {
            bool cancelled = cancel_repeating_timer(&timer);
            printf("cancelled... %d\n", cancelled);
            count++;
        }
    }
}
