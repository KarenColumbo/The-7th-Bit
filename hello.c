/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
// Our assembled program:
#include "hello.pio.h"
#define NUM_VOICES 2

#define USE_ADC_FM           // gpio 26 (adc 0)
const uint8_t RESET_PINS[NUM_VOICES] = {12, 13};
const uint8_t RANGE_PINS[NUM_VOICES] = {14, 15};
const uint8_t VOICE_TO_PIO[NUM_VOICES] = {0, 0};
const uint8_t VOICE_TO_SM[NUM_VOICES] = {0, 1};
const uint16_t DIV_COUNTER = 1250;
uint8_t RANGE_PWM_SLICES[NUM_VOICES];
float freq = 143.3312;
float lastfreq = 143.3312;

uint32_t LED_BLINK_START = 0;
PIO pio[2] = {pio0, pio1};

void init_sm(PIO pio, uint sm, uint offset, uint pin);
void set_frequency(PIO pio, uint sm, float freq);
void adc_task();

int main() {
    stdio_init_all();
    // pull GPIO 23 high for better accuracy
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 1);
    gpio_set_function(RANGE_PINS[0], GPIO_FUNC_PWM);
    RANGE_PWM_SLICES[0] = pwm_gpio_to_slice_num(RANGE_PINS[0]);
    pwm_set_wrap(RANGE_PWM_SLICES[0], DIV_COUNTER);
    pwm_set_enabled(RANGE_PWM_SLICES[0], true);

    // pio init
    uint offset[2];
    offset[0] = pio_add_program(pio[0], &hello_program);
    init_sm(pio[VOICE_TO_PIO[0]], VOICE_TO_SM[0], offset[VOICE_TO_PIO[0]], RESET_PINS[0]);
    adc_init();
    adc_gpio_init(26);

    // set frequency + range first time
    set_frequency(pio[VOICE_TO_PIO[0]], VOICE_TO_SM[0], freq);
    pwm_set_chan_level(RANGE_PWM_SLICES[0], pwm_gpio_to_channel(RANGE_PINS[0]), (int)(DIV_COUNTER*(freq*0.00025f-1/(100*freq))));


    while (1) {
        adc_task();
    }
}

void init_sm(PIO pio, uint sm, uint offset, uint pin) {
    init_sm_pin(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
}

void adc_task() {
    // Select ADC input 0 (GPIO26)
    adc_select_input(0);
    // Read the raw ADC value (0 to 4095)
    uint16_t adc_value = adc_read();
    printf("ADC Value: %d\n", adc_value);
    // Convert the raw ADC value to a frequency between 0 Hz and 16 kHz
    freq = (float)adc_value / 4095.0f * 16000.0f;    
    if (freq != lastfreq) {
        set_frequency(pio[VOICE_TO_PIO[0]], VOICE_TO_SM[0], freq);
        pwm_set_chan_level(RANGE_PWM_SLICES[1], pwm_gpio_to_channel(RANGE_PINS[1]), (int)(DIV_COUNTER*(freq*0.00025f-1/(100*freq))));
        lastfreq = freq;
    }
}

void set_frequency(PIO pio, uint sm, float freq) {
    if (freq == 0) {
        pio_sm_put(pio, sm, 0);
    } else {
        uint32_t clk_div = clock_get_hz(clk_sys) / 2 / freq;
        pio_sm_put(pio, sm, clk_div);
    }
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_y, 32));
}