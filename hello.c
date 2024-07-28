#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hello.pio.h"

#define NUM_VOICES 1
#define USE_ADC_FM           // gpio 26 (adc 0)

const uint8_t RESET_PINS[NUM_VOICES] = {15};
const uint8_t RANGE_PINS[NUM_VOICES] = {16};
const uint8_t VOICE_TO_PIO[NUM_VOICES] = {0};
const uint8_t VOICE_TO_SM[NUM_VOICES] = {0};
const uint16_t DIV_COUNTER = 1250;
uint8_t RANGE_PWM_SLICES[NUM_VOICES];
PIO pio[2] = {pio0, pio1};

void init_sm(PIO pio, uint sm, uint offset, uint pin);
void set_frequency(PIO pio, uint sm, float freq);

int main() {
    stdio_init_all();

    gpio_set_function(RANGE_PINS[0], GPIO_FUNC_PWM);
    RANGE_PWM_SLICES[1] = pwm_gpio_to_slice_num(RANGE_PINS[0]);
    pwm_set_wrap(RANGE_PWM_SLICES[1], DIV_COUNTER);
    pwm_set_enabled(RANGE_PWM_SLICES[1], true);

    // pio init
    uint offset[2];
    offset[0] = pio_add_program(pio[0], &hello_program);
    offset[1] = pio_add_program(pio[1], &hello_program);
    init_sm(pio[VOICE_TO_PIO[0]], VOICE_TO_SM[0], offset[VOICE_TO_PIO[0]], RESET_PINS[0]);

    while (1) {
        //adc_task();
        for (int i = 8; i < 16000; i += 10) {
            set_frequency(pio[VOICE_TO_PIO[0]], VOICE_TO_SM[0], i);
            pwm_set_chan_level(RANGE_PWM_SLICES[1], pwm_gpio_to_channel(RANGE_PINS[0]), (int)(DIV_COUNTER * (i * 0.00025f - 1 / (100 * i))));
            sleep_ms(10); // wait 10 ms
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
