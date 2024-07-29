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

#define VMAX 4.096            // Maximum output voltage for the DAC
float factor = VMAX / 15992.0; // DAC starting at 0 volts
#define SPI_PORT spi0
#define PIN_SCK 2
#define PIN_MOSI 3
#define PIN_CS 5

const uint8_t RESET_PIN = 15;
const uint8_t VOICE_TO_PIO = 0;
const uint8_t VOICE_TO_SM = 0;
const uint16_t DIV_COUNTER = 1250;
PIO pio[2] = {pio0, pio1};

void init_sm(PIO pio, uint sm, uint offset, uint pin);
void set_frequency(PIO pio, uint sm, float freq);
void set_range(float freq);

int main() {
    stdio_init_all();

    // Initialize SPI
    spi_init(SPI_PORT, 1000000);  // 1 MHz
    // Set SPI format to 8 bits
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    // Initialize SPI GPIO pins
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    // Initialize CS pin
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);  // CS high

    // pio init
    uint offset[2];
    offset[0] = pio_add_program(pio[0], &hello_program);
    offset[1] = pio_add_program(pio[1], &hello_program);
    init_sm(pio[VOICE_TO_PIO], VOICE_TO_SM, offset[VOICE_TO_PIO], RESET_PIN);

    while (1) {
        for (int i = 8; i <= 16000; i += 10) {
            set_frequency(pio[0], VOICE_TO_SM, (float)i);
            set_range((float)i);
            sleep_ms(25); 
        }
    }
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

void set_range(float freq) {
    
    // Normalize voltage to 12-bit DAC value
    uint16_t value = (uint16_t)((((freq - 8) * factor) / VMAX) * 4095);  // 12-bit value

    uint16_t command = 0b0001000000000000 | (value & 0x0FFF);  // 0b0001: Output A = 0, Dont Care = 0, 2 x Gain = 0, Shutdown off = 1
    uint8_t high_byte = command >> 8;
    uint8_t low_byte = command & 0xFF;

    gpio_put(PIN_CS, 0);  // CS low
    spi_write_blocking(SPI_PORT, &high_byte, 1);
    spi_write_blocking(SPI_PORT, &low_byte, 1);
    gpio_put(PIN_CS, 1);  // CS high
}
