#include "pico/stdlib.h"
#include <stdio.h>

int main() {
    stdio_init_all();

    sleep_ms(2000);

	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

	gpio_put(PICO_DEFAULT_LED_PIN, 1);

    printf("🚀 Pico online!\n");

    while (true)
    {
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(500);
    }
    
    return 0;
}