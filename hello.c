#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/spi.h"
#include "hello.pio.h"

void set_resistance(uint gpio_cs, uint16_t value);

#define CLOCK_PIN_1 15  // PIO clock pin (GPIO 15, physical pin 20 on Raspberry Pi Pico)
#define VOICE_TO_PIO_1 0
#define VOICE_TO_SM_1 0
#define SPI_PORT spi0
#define NUM_POTS 7
#define ENCODER_PIN_A 10  // GPIO 10 for encoder CLK
#define ENCODER_PIN_B 11  // GPIO 11 for encoder DT
#define ENCODER_PIN_SW 12 // GPIO 12 for encoder switch
#define LED_PIN 13  // GPIO 13 for the LED

// Define chip select pins for each MCP4151
#define PIN_CS1 16  // Chip select for MCP4151 (GPIO 16, physical pin 21 on Raspberry Pi Pico)
#define PIN_CS2 17  // Chip select for MCP4151 (GPIO 17, physical pin 22 on Raspberry Pi Pico)
#define PIN_CS3 21  // Chip select for MCP4151 (GPIO 21, physical pin 27 on Raspberry Pi Pico)
#define PIN_CS4 22  // Chip select for MCP4151 (GPIO 22, physical pin 29 on Raspberry Pi Pico)
#define PIN_CS5 26  // Chip select for MCP4151 (GPIO 26, physical pin 31 on Raspberry Pi Pico)
#define PIN_CS6 27  // Chip select for MCP4151 (GPIO 27, physical pin 32 on Raspberry Pi Pico)
#define PIN_CS7 28  // Chip select for MCP4151 (GPIO 28, physical pin 34 on Raspberry Pi Pico)

#define PIN_SCK 2  // Clock for SPI (GPIO 2, physical pin 4 on Raspberry Pi Pico)
#define PIN_MOSI 3  // MOSI for SPI (GPIO 3, physical pin 5 on Raspberry Pi Pico)

#define DIGIPOT_MAX_RESISTANCE 256  // MCP4151 has 257 steps (0-256)
#define RESISTANCE_STEP (DIGIPOT_MAX_RESISTANCE / 100)  // Steps corresponding to 1kΩ increments
#define DEBOUNCE_DELAY_MS 50
#define ENCODER_DEBOUNCE_DELAY_MS 5
#define RESET_DELAY_MS 1000

// Array of CS pins for the pots
const uint cs_pins[NUM_POTS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4, PIN_CS5, PIN_CS6, PIN_CS7};

// Default and current values for the pots
const uint def_vals[NUM_POTS] = {4, 8, 16, 32, 64, 128, 256};
uint16_t pot_vals[NUM_POTS] = {4, 8, 16, 32, 64, 128, 256};

// Current state
uint8_t selected_pot = 0;  // Index of the selected pot
bool value_select_mode = false;  // false = POT SELECT, true = VALUE SELECT

bool debounce_switch() {
    static uint32_t press_start_time = 0;
    static bool button_held = false;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (!gpio_get(ENCODER_PIN_SW)) {  // Button is pressed
        if (press_start_time == 0) {
            press_start_time = current_time;  // Record the time when the press started
        }
        
        if (!button_held && (current_time - press_start_time) > RESET_DELAY_MS) {
            // If the button has been held down long enough, reset pots
            init_pots();
            button_held = true;  // Prevent further triggering until the button is released
        }
        
        return false;  // Don't toggle modes during a long press
    } else {
        if (press_start_time != 0 && !button_held && (current_time - press_start_time) > DEBOUNCE_DELAY_MS) {
            // Button was released after a short press, so toggle modes
            press_start_time = 0;
            return true;
        } else {
            // Button was released after a long press, reset tracking variables
            press_start_time = 0;
            button_held = false;
            return false;
        }
    }
}

int8_t debounce_encoder() {
    static int last_state = 0;
    static uint32_t last_read_time = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    int current_state = (gpio_get(ENCODER_PIN_A) << 1) | gpio_get(ENCODER_PIN_B);

    if (current_state != last_state && (current_time - last_read_time) > ENCODER_DEBOUNCE_DELAY_MS) {
        last_read_time = current_time;
        last_state = current_state;

        if (((last_state == 0b00) && (current_state == 0b01)) ||
            ((last_state == 0b01) && (current_state == 0b11)) ||
            ((last_state == 0b11) && (current_state == 0b10)) ||
            ((last_state == 0b10) && (current_state == 0b00))) {
            return 1;  // Clockwise
        } else {
            return -1; // Counterclockwise
        }
    }
    return 0;
}

// Function to update the selected pot's resistance
void update_pot(uint8_t pot_index, uint16_t value) {
    pot_vals[pot_index] = value;
    set_resistance(cs_pins[pot_index], value);
}

void init_led() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);  // Start with the LED off
}

void init_encoder() {
    gpio_init(ENCODER_PIN_A);
    gpio_set_dir(ENCODER_PIN_A, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_A);

    gpio_init(ENCODER_PIN_B);
    gpio_set_dir(ENCODER_PIN_B, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_B);

    gpio_init(ENCODER_PIN_SW);
    gpio_set_dir(ENCODER_PIN_SW, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_SW);
}

// Function to handle encoder input
void handle_encoder_input(int encoder_delta, bool button_pressed) {
    if (button_pressed) {
        // Toggle between POT SELECT and VALUE SELECT mode
        value_select_mode = !value_select_mode;

        // Update LED state based on the mode
        gpio_put(LED_PIN, value_select_mode ? 0 : 1);  // LED on for POT SELECT, off for VALUE SELECT
    } else {
        if (value_select_mode) {
            // Adjust the selected pot's value
            int new_value = pot_vals[selected_pot] + encoder_delta;
            if (new_value >= 0 && new_value <= 255) {  // Ensure the value is within range
                update_pot(selected_pot, new_value);
            }
        } else {
            // Change the selected pot
            selected_pot = (selected_pot + encoder_delta + NUM_POTS) % NUM_POTS;
        }
    }
}

const float midi_frequencies[128] = {
    8.1758, 8.6620, 9.1770, 9.7227, 10.3009, 10.9134, 11.5623, 12.2499, 
    12.9783, 13.7500, 14.5676, 15.4339, 16.3516, 17.3239, 18.3540, 19.4454, 
    20.6017, 21.8268, 23.1247, 24.4997, 25.9565, 27.5000, 29.1352, 30.8677, 
    32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 
    51.9131, 55.0000, 58.2705, 61.7354, 65.4064, 69.2957, 73.4162, 77.7817, 
    82.4069, 87.3071, 92.4986, 97.9989, 103.8262, 110.0000, 116.5409, 123.4708, 
    130.8128, 138.5913, 146.8324, 155.5635, 164.8138, 174.6141, 184.9972, 195.9977, 
    207.6523, 220.0000, 233.0819, 246.9417, 261.6255, 277.1826, 293.6648, 311.1270, 
    329.6276, 349.2282, 369.9944, 391.9954, 415.3047, 440.0000, 466.1638, 493.8833, 
    523.2511, 554.3653, 587.3295, 622.2540, 659.2551, 698.4565, 739.9888, 783.9909, 
    830.6094, 880.0000, 932.3275, 987.7666, 1046.5023, 1108.7305, 1174.6591, 1244.5079, 
    1318.5102, 1396.9129, 1479.9777, 1567.9817, 1661.2188, 1760.0000, 1864.6550, 1975.5332, 
    2093.0045, 2217.4610, 2349.3181, 2489.0159, 2637.0205, 2793.8259, 2959.9554, 3135.9635, 
    3322.4376, 3520.0000, 3729.3101, 3951.0664, 4186.0090, 4434.9220, 4698.6363, 4978.0317, 
    5274.0409, 5587.6518, 5919.9108, 6271.9265, 6644.8752, 7040.0000, 7458.6202, 7902.1328, 
    8372.0181, 8869.8440, 9397.2726, 9956.0635, 10548.0818, 11175.3034, 11839.8215, 12543.8540
};

PIO pio[2] = {pio0, pio1};

void init_spi() {
    spi_init(SPI_PORT, 1000000);  // SPI at 1 MHz
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
}

// Function to set the resistance on a specified MCP4151
void set_resistance(uint gpio_cs, uint16_t value) {
    uint8_t command = 0x00;  // Command for writing to potentiometer
    uint8_t wiper_value = 256 - value;  // Reverse the value to increase resistance
    gpio_put(gpio_cs, 0);  // Select chip (CS low)
    spi_write_blocking(SPI_PORT, &command, 1);  // Send command byte
    spi_write_blocking(SPI_PORT, &wiper_value, 1);  // Send reversed value
    gpio_put(gpio_cs, 1);  // Deselect chip (CS high)
}

void init_chip_select_pins() {
    for (int i = 0; i < NUM_POTS; i++) {
        gpio_init(cs_pins[i]);             // Initialize the chip select pin
        gpio_set_dir(cs_pins[i], GPIO_OUT); // Set the pin direction to output
        gpio_put(cs_pins[i], 1);            // Set the pin high (deselect the chip)
    }
}

void init_pots() {
    for (int i = 0; i < NUM_POTS; i++) {
        set_resistance(cs_pins[i], def_vals[i]);  // Initialize each pot with its default value
    }
}

void init_sm(PIO pio, uint sm, uint offset, uint pin) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config c = hello_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin, 1);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void set_ramp_frequency(PIO pio, uint sm, float ramp_freq) {
    float clock_freq = ramp_freq * 128;
    uint32_t clk_div = clock_get_hz(clk_sys) / 2 / clock_freq;
    if (clock_freq == 0) clk_div = 0;
    pio_sm_put_blocking(pio, sm, clk_div);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_y, 32));
}

/*void set_random_resistances() {
    // Generate and set random values for each potentiometer
    set_resistance(PIN_CS1, rand() % 256);
    set_resistance(PIN_CS2, rand() % 256);
    set_resistance(PIN_CS3, rand() % 256);
    set_resistance(PIN_CS4, rand() % 256);
    set_resistance(PIN_CS5, rand() % 256);
    set_resistance(PIN_CS6, rand() % 256);
    set_resistance(PIN_CS7, rand() % 256);
} */

int main() {
    stdio_init_all();
    init_spi();  // Initialize SPI for MCP4151
    init_chip_select_pins();  // Initialize chip select pins
    init_pots();  // Set initial resistance values
    init_led();   // Initialize the LED

    // Initialize the PIO state machine for ramp output
    uint offset = pio_add_program(pio[0], &hello_program);
    init_sm(pio[VOICE_TO_PIO_1], VOICE_TO_SM_1, offset, CLOCK_PIN_1);
    set_ramp_frequency(pio[0], VOICE_TO_SM_1, 220);

    while (1) {
        int8_t encoder_delta = debounce_encoder();
        bool button_pressed = debounce_switch();
        handle_encoder_input(encoder_delta, button_pressed);
        sleep_ms(10);  // Add a small delay to avoid busy-waiting
    }

    return 0;
}
