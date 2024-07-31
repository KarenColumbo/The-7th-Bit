#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hello.pio.h"

#define CLOCK_PIN 15
#define VOICE_TO_PIO 0
#define VOICE_TO_SM 0

const float midi_frequencies[128] = {
    8.18, 8.66, 9.18, 9.72, 10.30, 10.91, 11.56, 12.25, 12.98, 13.75, 14.57, 15.43,
    16.35, 17.32, 18.35, 19.45, 20.60, 21.83, 23.12, 24.50, 25.96, 27.50, 29.14, 30.87,
    32.70, 34.65, 36.71, 38.89, 41.20, 43.65, 46.25, 49.00, 51.91, 55.00, 58.27, 61.74,
    65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 116.54, 123.47,
    130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08, 246.94,
    261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88,
    523.25, 554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99, 830.61, 880.00, 932.33, 987.77,
    1046.50, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760.00, 1864.66, 1975.53,
    2093.00, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520.00, 3729.31, 3951.07,
    4186.01, 4434.92, 4698.64, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040.00, 7458.62, 7902.13,
    8372.02, 8869.84, 9397.27, 9956.06, 10548.08, 11175.30, 11839.82, 12543.85
};

PIO pio[2] = {pio0, pio1};

void init_sm(PIO pio, uint sm, uint offset, uint pin) {
    // Initialize the specified GPIO pin for PIO control
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    
    // Get the default configuration for the PIO program
    pio_sm_config c = hello_program_get_default_config(offset);
    // Configure the state machine to control the specified pin
    sm_config_set_set_pins(&c, pin, 1);
    
    // Initialize the state machine with the specified configuration
    pio_sm_init(pio, sm, offset, &c);
    // Enable the state machine
    pio_sm_set_enabled(pio, sm, true);
}

void set_ramp_frequency(PIO pio, uint sm, float ramp_freq) {
    // 256 steps per ramp cycle for an 8-bit counter
    float clock_freq = ramp_freq * 256;
    uint32_t clk_div = clock_get_hz(clk_sys) / 2 / clock_freq;
    if (clock_freq == 0) clk_div = 0;

    // Write to the PIO FIFO and execute commands to update the divider
    pio_sm_put_blocking(pio, sm, clk_div);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_y, 32));
}

int main() {
    stdio_init_all();

    // PIO init
    uint offset = pio_add_program(pio[0], &hello_program);
    init_sm(pio[VOICE_TO_PIO], VOICE_TO_SM, offset, CLOCK_PIN);

    while (1) {
        // Loop through a range of frequencies from 8 Hz to 16 kHz
        for (int i = 0; i <= 127; i++) {
            set_ramp_frequency(pio[0], VOICE_TO_SM, (float)midi_frequencies[i]);
            printf("Setting ramp wave frequency to %d Hz with 8-bit resolution\n", midi_frequencies[i]);
            sleep_ms(2000);  // Pause for 5 seconds
        }
    }
}
