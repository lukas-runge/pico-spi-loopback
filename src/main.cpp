#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdio.h>

#define RASPI_MOSI_PIN 16
#define RASPI_CS_PIN 17
#define RASPI_SCLK_PIN 18
#define RASPI_MISO_PIN 19
#define RASPI_INT_MISO_PIN 20
#define RASPI_INT_MOSI_PIN 21
spi_inst_t* raspi_spi = spi0;

int main() {
    stdio_init_all();

    // setup spi to raspi
    spi_init(raspi_spi, 15'625'000 / 2);
    spi_set_slave(raspi_spi, true);
    gpio_set_function(RASPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_CS_PIN, GPIO_FUNC_SPI);

    spi_set_format(raspi_spi, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    uint8_t tx = 0x00;
    uint8_t rx;
    while (true) {
        spi_write_read_blocking(raspi_spi, &tx, &rx, 1);
        tx = rx;
    }
    
    return 0;
}
