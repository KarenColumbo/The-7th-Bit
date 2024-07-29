#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hello.pio.h"

#define SPI_PORT spi0
#define PIN_SCK 2
#define PIN_MOSI 3
#define PIN_CS0 5
#define VMAX 3.3            // Maximum output voltage for the DAC
float factor = VMAX / 15992.0;

const uint8_t RESET_PIN = 15;
const uint8_t VOICE_TO_PIO = 0;
const uint8_t VOICE_TO_SM = 0;
const uint16_t DIV_COUNTER = 1250;
PIO pio[2] = {pio0, pio1};

void print_binary(uint16_t number) {
    for (int i = 15; i >= 0; i--) {
        printf("%d", (number >> i) & 1);
    }
    printf("\n");
}

void init_sm(PIO pio, uint sm, uint offset, uint pin) {
    init_sm_pin(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
}

void set_frequency(PIO pio, uint sm, float freq) {
    uint32_t clk_div = clock_get_hz(clk_sys) / 2 / freq;
    if (freq == 0) clk_div = 0;
    pio_sm_put(pio, sm, clk_div);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_y, 32));
}

void set_range(uint8_t dacpin, uint16_t freq) {
    uint16_t value = (uint16_t)((((freq - 8) * factor) / VMAX) * 4095);  // 12-bit value
    // MCP4822 command: Write to DAC A, gain = 2x, active mode
    uint16_t command = 0b0001000000000000 | (value & 0x0FFF);  // 0b00100000 = A/B = 0, Gain = 0 (2x), Shutdown = 1
    uint8_t high_byte = command >> 8;
    uint8_t low_byte = command & 0xFF;

    // Print the 16-bit command in binary
    printf("Command: ");
    print_binary(command);

    gpio_put(dacpin, 0);  // CS low
    spi_write_blocking(SPI_PORT, &high_byte, 1);
    spi_write_blocking(SPI_PORT, &low_byte, 1);
    gpio_put(dacpin, 1);  // CS high
}

int main() {
    stdio_init_all();

    spi_init(SPI_PORT, 1000000);  // 1 MHz
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_init(PIN_CS0);
    gpio_set_dir(PIN_CS0, GPIO_OUT);
    gpio_put(PIN_CS0, 1);  

    // pio init
    uint offset[2];
    offset[0] = pio_add_program(pio[0], &hello_program);
    offset[1] = pio_add_program(pio[1], &hello_program);
    init_sm(pio[VOICE_TO_PIO], VOICE_TO_SM, offset[VOICE_TO_PIO], RESET_PIN);

    while (1) {
        for (int testfreq = 8; testfreq <= 6000; testfreq += 250) {
            set_frequency(pio[0], VOICE_TO_SM, (float)testfreq);
            set_range(PIN_CS0, testfreq);  // Set voltage value
            printf("Setting voltage for frequency %d\n", testfreq);
            sleep_ms(10000);  // Pause for 10 seconds
        }
    }
}
