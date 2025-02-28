// -------------------------------------------------- //
// This file is autogenerated by pioasm; do not edit! //
// -------------------------------------------------- //

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// -------- //
// encoders //
// -------- //

#define encoders_wrap_target 0
#define encoders_wrap 22

static const uint16_t encoders_program_instructions[] = {
            //     .wrap_target
    0x4002, //  0: in     pins, 2                    
    0xa046, //  1: mov    y, isr                     
    0xe020, //  2: set    x, 0                       
    0xa0c1, //  3: mov    isr, x                     
    0x4002, //  4: in     pins, 2                    
    0xa026, //  5: mov    x, isr                     
    0x00a8, //  6: jmp    x != y, 8                  
    0x0002, //  7: jmp    2                          
    0xa027, //  8: mov    x, osr                     
    0xa0e2, //  9: mov    osr, y                     
    0x6041, // 10: out    y, 1                       
    0x008e, // 11: jmp    y--, 14                    
    0x00cf, // 12: jmp    pin, 15                    
    0x0013, // 13: jmp    19                         
    0x00d3, // 14: jmp    pin, 19                    
    0xa029, // 15: mov    x, !x                      
    0x0051, // 16: jmp    x--, 17                    
    0xa029, // 17: mov    x, !x                      
    0x0014, // 18: jmp    20                         
    0x0054, // 19: jmp    x--, 20                    
    0xa0c1, // 20: mov    isr, x                     
    0x8020, // 21: push   block                      
    0xa0e1, // 22: mov    osr, x                     
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program encoders_program = {
    .instructions = encoders_program_instructions,
    .length = 23,
    .origin = -1,
};

static inline pio_sm_config encoders_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + encoders_wrap_target, offset + encoders_wrap);
    return c;
}

static inline void encoders_program_init(PIO pio, uint sm, uint offset, uint pin, bool debounce) {
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 2, false);
    pio_gpio_init(pio, pin);
    pio_gpio_init(pio, pin+1);
    gpio_pull_up(pin);
    gpio_pull_up(pin+1);
    pio_sm_config c = encoders_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_jmp_pin(&c, pin +1);
    // Shift to left, autopull disabled
    sm_config_set_in_shift(&c, false, false, 2);
    // Debounce via reduced clock
    if (debounce)
        sm_config_set_clkdiv(&c, 5000);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

#endif

