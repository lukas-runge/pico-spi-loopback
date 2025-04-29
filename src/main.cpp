#include <stdio.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"

#define RASPI_MOSI_PIN 16
#define RASPI_CS_PIN 17
#define RASPI_SCLK_PIN 18
#define RASPI_MISO_PIN 19
#define RASPI_INT_MISO_PIN 20
#define RASPI_INT_MOSI_PIN 21
#define RASPI_SPI spi0

int main() {
    stdio_init_all();

    sleep_ms(2000);

    // setup spi to raspi
    spi_init(RASPI_SPI, 15'625'000 / 2);
    spi_set_slave(RASPI_SPI, true);
    gpio_set_function(RASPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_CS_PIN, GPIO_FUNC_SPI);

    spi_set_format(RASPI_SPI, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    uint8_t buffer = 0xFF;
    while (true) {
        spi_write_read_blocking(RASPI_SPI, &buffer, &buffer, 1);
    }

    return 0;
}
