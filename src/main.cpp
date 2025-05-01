#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "hardware/dma.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#define RASPI_MOSI_PIN 16
#define RASPI_CS_PIN 17
#define RASPI_SCLK_PIN 18
#define RASPI_MISO_PIN 19
#define RASPI_INT_MISO_PIN 20
#define RASPI_INT_MOSI_PIN 21
#define RASPI_SPI spi0

queue_t sendQ;
queue_t recvQ;

typedef struct {
    uint8_t data[32];
} message_t;

uint8_t message[32];
uint8_t message_len = 0;

static uint8_t data[32];
static uint32_t len;

void readSPI() {
    spi_write_read_blocking(RASPI_SPI, data, message, 32);

    for (int i = 0; i < sizeof(message); i++) {
        printf("%02X ", message[i]);
    }
    printf("\n");
}

void writeSPI(uint8_t *data, uint8_t len) {
    gpio_put(RASPI_INT_MISO_PIN, 1);
    spi_write_blocking(RASPI_SPI, data, len);
    gpio_put(RASPI_INT_MISO_PIN, 0);
}

void coprocessing() {
    // setup spi to raspi
    spi_init(RASPI_SPI, 15'625'000 / 2);
    spi_set_slave(RASPI_SPI, true);
    gpio_set_function(RASPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_CS_PIN, GPIO_FUNC_SPI);

    spi_set_format(RASPI_SPI, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    // INT Pins
    gpio_init(RASPI_INT_MISO_PIN);
    gpio_set_dir(RASPI_INT_MISO_PIN, GPIO_OUT);
    gpio_put(RASPI_INT_MISO_PIN, 0);

    static uint8_t rcvBuffer[32];
    static uint8_t sendBuffer[32];
    uint32_t pos = 0;



    message_t sendMsg;

    bool waitingOnWrite = false;

    uint8_t counter = 0;
    
    
    while (true) {
        uint32_t start = to_us_since_boot(get_absolute_time());
        
        if (!queue_is_empty(&sendQ) && !waitingOnWrite) {
            if (!queue_try_peek(&sendQ, &sendMsg)) {
                continue;
            }
            
            memset(sendBuffer, ++counter, sizeof(sendBuffer));
            printf("Sending: ");
            for (int i = 0; i < sizeof(sendBuffer); i++) {
                printf("%02X ", sendBuffer[i]);
            }
            printf("\n");

            const uint dma_rx = dma_claim_unused_channel(true);
            const uint dma_tx = dma_claim_unused_channel(true);
        
            // We set the outbound DMA to transfer from a memory buffer to the SPI transmit FIFO paced by the SPI TX FIFO DREQ
            // The default is for the read address to increment every element (in this case 1 byte = DMA_SIZE_8)
            // and for the write address to remain unchanged.
            printf("Configure TX DMA\n");
            dma_channel_config c = dma_channel_get_default_config(dma_tx);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
            channel_config_set_dreq(&c, spi_get_dreq(RASPI_SPI, true));
            dma_channel_configure(dma_tx, &c,
                                  &spi_get_hw(RASPI_SPI)->dr,  // write address
                                  sendBuffer,                  // read address
                                  32,                          // element count (each element is of size transfer_data_size)
                                  false);                      // don't start yet
        
            printf("Configure RX DMA\n");
        
            // We set the inbound DMA to transfer from the SPI receive FIFO to a memory buffer paced by the SPI RX FIFO DREQ
            // We configure the read address to remain unchanged for each element, but the write
            // address to increment (so data is written throughout the buffer)
            c = dma_channel_get_default_config(dma_rx);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
            channel_config_set_dreq(&c, spi_get_dreq(RASPI_SPI, false));
            channel_config_set_read_increment(&c, false);
            channel_config_set_write_increment(&c, true);
            dma_channel_configure(dma_rx, &c,
                                  rcvBuffer,                   // write address
                                  &spi_get_hw(RASPI_SPI)->dr,  // read address
                                  32,                          // element count (each element is of size transfer_data_size)
                                  false);                      // don't start yet

            gpio_put(RASPI_INT_MISO_PIN, 1);

            printf("Starting DMAs...\n");
            // start them exactly simultaneously to avoid races (in extreme cases the FIFO could overflow)
            dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
            printf("Wait for RX complete...\n");
            dma_channel_wait_for_finish_blocking(dma_rx);
            if (dma_channel_is_busy(dma_tx)) {
                panic("RX completed before TX");
            }

            printf("Done. Checking...");
            for (int i = 0; i < sizeof(rcvBuffer); i++) {
                printf("%02X ", rcvBuffer[i]);
            }
            printf("\n");

            gpio_put(RASPI_INT_MISO_PIN, 0);

            queue_remove_blocking(&sendQ, &sendMsg);

            // dma_channel_cleanup(dma_rx);
            // dma_channel_cleanup(dma_tx);

            dma_channel_unclaim(dma_rx);
            dma_channel_unclaim(dma_tx);
        }

        // sleep_ms(100);

        // while (spi_is_readable(RASPI_SPI) && pos < sizeof(rcvBuffer)) {
        //     spi_write_read_blocking(RASPI_SPI, &sendBuffer[pos], &rcvBuffer[pos], 1);
        //     pos++;
        // }

        // pos = 0;


        // if (waitingOnWrite && spi_is_writable(RASPI_SPI)) {
        //     if (!queue_try_remove(&sendQ, &sendMsg)) {
        //         gpio_put(RASPI_INT_MISO_PIN, 0);
        //         waitingOnWrite = false;
        //         continue;
        //     }

        //     // printf("Sending: ");
        //     // for (int i = 0; i < len; i++) {
        //     //     printf("%02X ", data[i]);
        //     // }
        //     // printf("\n");
        //     writeSPI(data, len);

        //     // gpio_put(RASPI_INT_MISO_PIN, 0);
        //     waitingOnWrite = false;
        // }
        uint32_t end = to_us_since_boot(get_absolute_time());
        uint32_t loop_time = end - start;
        // printf("Coprocessing Loop Time: %d us\n", loop_time);
    }
}

int main() {
    stdio_init_all();

    sleep_ms(2000);

    // setup internal led
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);

    // setup spi to raspi
    spi_init(RASPI_SPI, 15'625'000 / 2);
    spi_set_slave(RASPI_SPI, true);
    gpio_set_function(RASPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RASPI_CS_PIN, GPIO_FUNC_SPI);

    spi_set_format(RASPI_SPI, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    // INT Pins
    gpio_init(RASPI_INT_MISO_PIN);
    gpio_set_dir(RASPI_INT_MISO_PIN, GPIO_OUT);
    gpio_put(RASPI_INT_MISO_PIN, 0);

    queue_init(&sendQ, sizeof(message_t), 10);
    queue_init(&recvQ, sizeof(message_t), 10);

    sleep_ms(1000);

    multicore_launch_core1(coprocessing);

    sleep_ms(1000);

    uint32_t min_loop_time = 100;
    uint32_t max_loop_time = 0;
    uint32_t avg_loop_time;

    while (true) {
        uint32_t start = to_us_since_boot(get_absolute_time());
        // if (spi_is_readable(RASPI_SPI)) readSPI();

        if (to_ms_since_boot(get_absolute_time()) % 500 > 250) {
            if (gpio_get(PICO_DEFAULT_LED_PIN)) {
                gpio_put(PICO_DEFAULT_LED_PIN, 0);
                // writeSPI((uint8_t *)"Hello", 5);
                message_t msg;
                memcpy(msg.data, "Hello", 5);
                queue_try_add(&sendQ, &msg);
            }
        } else {
            if (!gpio_get(PICO_DEFAULT_LED_PIN)) {
                gpio_put(PICO_DEFAULT_LED_PIN, 1);
                // writeSPI((uint8_t *)"World", 5);
                message_t msg;
                memcpy(msg.data, "World", 5);
                queue_try_add(&sendQ, &msg);
            }
        }
        uint32_t end = to_us_since_boot(get_absolute_time());
        uint32_t loop_time = end - start;
        if (loop_time < min_loop_time) {
            min_loop_time = loop_time;
        }
        if (loop_time > max_loop_time) {
            max_loop_time = loop_time;
        }
        avg_loop_time = (avg_loop_time * 99) / 100 + loop_time / 100;

        // printf("Loop Time: %d us, Min: %d us, Max: %d us, Avg: %d us\n", loop_time, min_loop_time, max_loop_time, avg_loop_time);
    }

    return 0;
}
