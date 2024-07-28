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

#define VMAX 4.0            // Maximum output voltage for the DAC
float factor = VMAX / 15992.0;
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
    spi_init(SPI_PORT, 10000000);  // 10 MHz
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

    // MCP4822 command: Write to DAC A, buffer off, gain = 1x, active mode
    uint8_t dac_cmd = 0b00010000;  // 0b00010000: A/B = 0, Buffer = 0, Gain x 2 = 0, Shutdown = 1
    uint8_t high_byte = (dac_cmd << 4) | (value >> 8);
    uint8_t low_byte = value & 0xFF;

    gpio_put(PIN_CS, 0);  // CS low
    spi_write_blocking(SPI_PORT, &high_byte, 1);
    spi_write_blocking(SPI_PORT, &low_byte, 1);
    gpio_put(PIN_CS, 1);  // CS high
}
