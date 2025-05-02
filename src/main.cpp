#include <stdio.h>

#include "hardware/dma.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define MOSI_PIN 16
#define CS_PIN 17
#define SCLK_PIN 18
#define MISO_PIN 19
#define SPI spi0

int main() {
    stdio_init_all();

    spi_init(SPI, 1'000'000);
    spi_set_slave(SPI, true);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(CS_PIN, GPIO_FUNC_SPI);

    spi_set_format(SPI, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    const uint dma_ping = dma_claim_unused_channel(true);
    const uint dma_pong = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(dma_ping);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(SPI, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_chain_to(&c, dma_pong);
    dma_channel_configure(dma_ping, &c,
                          &spi_get_hw(SPI)->dr,
                          &spi_get_hw(SPI)->dr,
                          1,
                          false);

    c = dma_channel_get_default_config(dma_pong);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(SPI, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_chain_to(&c, dma_ping);
    dma_channel_configure(dma_pong, &c,
                          &spi_get_hw(SPI)->dr,
                          &spi_get_hw(SPI)->dr,
                          1,
                          true);

    while (true) {
        tight_loop_contents();
    }

    dma_channel_unclaim(dma_ping);
    dma_channel_unclaim(dma_pong);

    return 0;
}
