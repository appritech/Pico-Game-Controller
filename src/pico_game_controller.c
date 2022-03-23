/*
 * Pico Game Controller
 * @author Appri Custom Technologies based on Pico Game Controller from SpeedyPotato
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "encoders.pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "ws2812.pio.h"

#define SW_GPIO_SIZE 11               // Number of switches
#define LED_GPIO_SIZE 12              // Number of LEDs
#define ENC_GPIO_SIZE 2               // Number of encoders
#define ADC_GPIO_SIZE 3               // Number of analogs
#define ENC_PPR 600                   // Encoder PPR
#define ENC_DEBOUNCE true             // Encoder Debouncing
#define ENC_PULSE (ENC_PPR * 4)       // 4 pulses per PPR
#define ENC_ROLLOVER (ENC_PULSE * 2)  // Delta Rollover threshold
#define REACTIVE_TIMEOUT_MAX 500000  // Cycles before HID falls back to reactive
#define WS2812B_LED_SIZE 10          // Number of WS2812B LEDs
#define WS2812B_LED_ZONES 2          // Number of WS2812B LED Zones
#define WS2812B_LEDS_PER_ZONE \
  WS2812B_LED_SIZE / WS2812B_LED_ZONES  // Number of LEDs per zone

/*
// MODIFY KEYBINDS HERE, MAKE SURE LENGTHS MATCH SW_GPIO_SIZE - for KEYBOARD
const uint8_t SW_KEYCODE[] = {HID_KEY_D, HID_KEY_F, HID_KEY_J, HID_KEY_K,
                              HID_KEY_C, HID_KEY_M, HID_KEY_A, HID_KEY_B,
                              HID_KEY_1, HID_KEY_E, HID_KEY_G};
*/

// Note: these are the GP numbers from the Pico
// Changed numbers to match Pigeon GPs
const uint8_t SW_GPIO[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};
// Changed numbers to match Pigeon OUTs 
const uint8_t LED_GPIO[] = {
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22
};

  /*  Remove encoders for now - NOTE: will need to check GPIO numbers if reinstate
const uint8_t ENC_GPIO[] = {0, 2};      // L_ENC(0, 1); R_ENC(2, 3)
const bool ENC_REV[] = {false, false};  // Reverse Encoders
const uint8_t WS2812B_GPIO = 28;

PIO pio, pio_1;
uint32_t enc_val[ENC_GPIO_SIZE];
uint32_t prev_enc_val[ENC_GPIO_SIZE];
int cur_enc_val[ENC_GPIO_SIZE];
bool enc_changed;
*/
bool sw_val[SW_GPIO_SIZE];
bool prev_sw_val[SW_GPIO_SIZE];
bool sw_changed;

bool leds_changed;
//unsigned long reactive_timeout_count = REACTIVE_TIMEOUT_MAX;
unsigned long reactive_timeout_count = 0;   // problem if this is called immediately on starting & a switch is in - see what happens removing it
unsigned long old_reactive_timeout_count = 0;   // problem if this is called immediately on starting & a switch is in - see what happens removing it
unsigned long report_timer_count = 0;   // counter for how frequently to send the report

bool adc_changed;
uint8_t adc_val[ADC_GPIO_SIZE];
uint8_t prev_adc_val[ADC_GPIO_SIZE];

uint8_t LED_OnCount;    // debug tracker

void (*loop_mode)();

typedef struct {
  uint8_t r, g, b;
} RGB_t;

union {
  struct {
    uint8_t LEDs[LED_GPIO_SIZE];
    RGB_t rgb[WS2812B_LED_ZONES];
  } lights;
  uint8_t raw[LED_GPIO_SIZE + WS2812B_LED_ZONES * 3];
} lights_report;

/* Hide encoders for the moment
/**
 * WS2812B RGB Assignment
 * @param pixel_grb The pixel color to set
 **//*
static inline void put_pixel(uint32_t pixel_grb) 
{
  pio_sm_put_blocking(pio1, ENC_GPIO_SIZE, pixel_grb << 8u);
}

/**
 * WS2812B RGB Format Helper
 **//*
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) 
{
  return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

/**
 * 768 Color Wheel Picker
 * @param wheel_pos Color value, r->g->b->r...
 **//*
uint32_t color_wheel(uint16_t wheel_pos) 
{
  wheel_pos %= 768;
  if (wheel_pos < 256) {
    return urgb_u32(wheel_pos, 255 - wheel_pos, 0);
  } else if (wheel_pos < 512) {
    wheel_pos -= 256;
    return urgb_u32(255 - wheel_pos, 0, wheel_pos);
  } else {
    wheel_pos -= 512;
    return urgb_u32(0, wheel_pos, 255 - wheel_pos);
  }
}

/**
 * Color cycle effect
 **//*
void ws2812b_color_cycle(uint32_t counter) 
{
  for (int i = 0; i < WS2812B_LED_SIZE; ++i) 
  {
    put_pixel(color_wheel((counter + i * (int)(768 / WS2812B_LED_SIZE)) % 768));
  }
}

/**
 * WS2812B Lighting
 * @param counter Current number of WS2812B cycles
 **//*
void ws2812b_update(uint32_t counter) 
{
  if (reactive_timeout_count >= REACTIVE_TIMEOUT_MAX) 
  {
    ws2812b_color_cycle(counter);
  } else {
    for (int i = 0; i < WS2812B_LED_ZONES; i++) 
    {
      for (int j = 0; j < WS2812B_LEDS_PER_ZONE; j++) 
      {
        put_pixel(urgb_u32(lights_report.lights.rgb[i].r,
                           lights_report.lights.rgb[i].g,
                           lights_report.lights.rgb[i].b));
      }
    }
  }
}
*/


// Flash - use for debugging to see where code is reached
void flashLED(int count)
{
  for (int i = 0; i < count; i++)
  {
  gpio_put(25,0);  // if already off, this won't matter
  LED_OnCount = 0;
  sleep_ms(250);
  gpio_put(25,1);
  LED_OnCount = 200;     
  sleep_ms(300);
 }
  gpio_put(25,0);     // ensure it is off at the 
  LED_OnCount = 0;
  sleep_ms(500);      // and add a gap to show space between patterns 
}

// quick debug flash so not slowing it down but alternate every 10 ms so visible
void flipLED()
{
  LED_OnCount ++;
  if (LED_OnCount >= 200)
    LED_OnCount = 0;
  gpio_put(25,LED_OnCount>=100);     // ensure it is off at the end

}

// see https://www.i-programmer.info/programming/hardware/14849-the-pico-in-c-basic-pwm.html?start=2
// set up to run at frequency f, for d% of the time
uint32_t pwm_set_freq_duty(uint slice_num, uint chan,
                           uint32_t f, int d)
{
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / f / 4096 + (clock % (f * 4096) != 0);
    if (divider16 / 16 == 0)
        divider16 = 16;
    uint32_t wrap = clock * 16 / divider16 / f - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * d / 100);
    return wrap;
}

/**
 * HID/Reactive Lights
 **/
void update_lights() 
{
  /*
  if (reactive_timeout_count < REACTIVE_TIMEOUT_MAX) 
  {
    reactive_timeout_count++;
    if (reactive_timeout_count > 50000 && old_reactive_timeout_count < 50000)
    {
      flashLED(3);
    }
    old_reactive_timeout_count= reactive_timeout_count;
  }
  */

  if (leds_changed) 
  {
    //reactive_timeout_count = 0;   // just kill this for the moment and see what happens

/*    // just show this once, not every option
    if (reactive_timeout_count >= REACTIVE_TIMEOUT_MAX) 
    {
      flashLED(4);
    } 
    else 
    {
      flashLED(2);
    }
    */
    for (int i = 0; i < LED_GPIO_SIZE; i++) 
    {
      
      /*if (reactive_timeout_count >= REACTIVE_TIMEOUT_MAX) 
      {
        if (sw_val[i]) 
        {
          gpio_put(LED_GPIO[i], 1);
        } 
        else 
        {
          gpio_put(LED_GPIO[i], 0);
        }
        reactive_timeout_count = 0;     // will be reset if it is needed again
      } 
      else 
      */
      {
        if (lights_report.lights.LEDs[i] == 0) 
        {
          pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50, 0);// set all outputs to lowest as well
          pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), false);   // but turn them off
          gpio_set_function(LED_GPIO[i], GPIO_FUNC_SIO);    // stop PWM
 
          if (i==11)
            flashLED(2);
          gpio_put(LED_GPIO[i], 0);
        }
        else if (lights_report.lights.LEDs[i] == 100) 
        {
          pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50, 100); // set to highest so will start from that when changes
          pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), false);   // but turn them off - don't need PWM at this point
          gpio_set_function(LED_GPIO[i], GPIO_FUNC_SIO);    // stop PWM
          if (i==11)
            flashLED(2);
          gpio_put(LED_GPIO[i], 1);
        }
        else 
        {
          gpio_put(LED_GPIO[i], 1);
          gpio_set_function(LED_GPIO[i], GPIO_FUNC_PWM);    // start PWM again

          //pwm_set_chan_level(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]), lights_report.lights.LEDs[i]);    // set outputs to level we read
          pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50, lights_report.lights.LEDs[i] );// set all outputs fully on for the moment - adjust as needed later
          pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), true);   // and turn on
          if (i==11)
            flashLED(3);
        }
      }
    }
    leds_changed = false;
  }
}

struct report {
  uint16_t buttons;
  uint8_t adc0;
  uint8_t adc1;
  uint8_t adc2;     // Changing the size of the report is breaking it for now - need to look further
} report;

/**
 * Gamepad Mode
 **/
void joy_mode() 
{
  if (tud_hid_ready()) 
  {
    bool send_report = false;
    if (sw_changed) 
    {
      //send_report = true;
      uint16_t translate_buttons = 0;
      for (int i = SW_GPIO_SIZE - 1; i >= 0; i--) 
      {
        translate_buttons = (translate_buttons << 1) | (sw_val[i] ? 1 : 0);
        prev_sw_val[i] = sw_val[i];
      }
      report.buttons = translate_buttons;
      sw_changed = false;
    }

    if (adc_changed)
    {
        //send_report = true;   // seems to be over-sending, what if we just send when buttons send for the moment
        report.adc0 = adc_val[0];    
        report.adc1 = adc_val[1];
        report.adc2 = adc_val[2];
        prev_adc_val[0] = adc_val[0];
        prev_adc_val[1] = adc_val[1];
        prev_adc_val[2] = adc_val[2];
        adc_changed = false;
    }

    report_timer_count ++;
    if (report_timer_count > 1000)    // send it every so often anyway, as unity is expecting continuous reports but too often will kill the board 
    {
      send_report = true;
    }


/*
    if (enc_changed) {
      send_report = true;

      // find the delta between previous and current enc_val
      for (int i = 0; i < ENC_GPIO_SIZE; i++) {
        int delta;
        int changeType;                      // -1 for negative 1 for positive
        if (enc_val[i] > prev_enc_val[i]) {  // if the new value is bigger its
                                             // a positive change
          delta = enc_val[i] - prev_enc_val[i];
          changeType = 1;
        } else {  // otherwise its a negative change
          delta = prev_enc_val[i] - enc_val[i];
          changeType = -1;
        }
        // Overflow / Underflow
        if (delta > ENC_ROLLOVER) {
          // Reverse the change type due to overflow / underflow
          changeType *= -1;
          delta = UINT32_MAX - delta + 1;  // this should give us how much we
                                           // overflowed / underflowed by
        }

        cur_enc_val[i] =
            cur_enc_val[i] + ((ENC_REV[i] ? 1 : -1) * delta * changeType);
        while (cur_enc_val[i] < 0) {
          cur_enc_val[i] = ENC_PULSE - cur_enc_val[i];
        }

        prev_enc_val[i] = enc_val[i];
      }

      report.adc0 = ((double)cur_enc_val[0] / ENC_PULSE) * 256;
      report.adc1 = ((double)cur_enc_val[1] / ENC_PULSE) * 256;
      //report.adc2 = 0;
      enc_changed = false;
    }*/

    if (send_report) 
    {
      //flashLED();
      tud_hid_n_report(0x00, REPORT_ID_JOYSTICK, &report, sizeof(report));
      report_timer_count = 0;   // we sent, so reset the wait
      //flipLED();    // don't use the pausing version here
      /*
      uint8_t size = sizeof(report);
      flashLED(1);
      flashLED(size);
      flashLED(2);
      */
    }
  }
}

/**
 * Keyboard Mode
 **/
/*void key_mode() {
  if (tud_hid_ready()) {
    /*------------- Keyboard -------------*//*
    if (sw_changed) {
      uint8_t nkro_report[32] = {0};
      for (int i = 0; i < SW_GPIO_SIZE; i++) {
        if (sw_val[i]) {
          uint8_t bit = SW_KEYCODE[i] % 8;
          uint8_t byte = (SW_KEYCODE[i] / 8) + 1;
          if (SW_KEYCODE[i] >= 240 && SW_KEYCODE[i] <= 247) {
            nkro_report[0] |= (1 << bit);
          } else if (byte > 0 && byte <= 31) {
            nkro_report[byte] |= (1 << bit);
          }

          prev_sw_val[i] = sw_val[i];
        }
      }
      // Send key report
      tud_hid_n_report(0x00, REPORT_ID_KEYBOARD, &nkro_report,
                       sizeof(nkro_report));
      sw_changed = false;
    }
    */

    /*------------- Mouse -------------*/
    /*
    if (enc_changed) {
      // Delay if needed before attempt to send mouse report
      while (!tud_hid_ready()) {
        board_delay(1);
      }
      // find the delta between previous and current enc_val
      int delta[ENC_GPIO_SIZE] = {0};
      for (int i = 0; i < ENC_GPIO_SIZE; i++) {
        int changeType;                      // -1 for negative 1 for positive
        if (enc_val[i] > prev_enc_val[i]) {  // if the new value is bigger its
                                             // a positive change
          delta[i] = enc_val[i] - prev_enc_val[i];
          changeType = 1;
        } else {  // otherwise its a negative change
          delta[i] = prev_enc_val[i] - enc_val[i];
          changeType = -1;
        }
        // Overflow / Underflow
        if (delta[i] > ENC_ROLLOVER) {
          // Reverse the change type due to overflow / underflow
          changeType *= -1;
          delta[i] =
              UINT32_MAX - delta[i] + 1;  // this should give us how much we
                                          // overflowed / underflowed by
        }
        delta[i] *= changeType * (ENC_REV[i] ? 1 : -1);  // set direction
        prev_enc_val[i] = enc_val[i];
      }
      tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta[0], delta[1], 0, 0);
      enc_changed = false;
    }
    
  }
}
*/

union converted16{
        struct{
                uint8_t lsb;
                uint8_t msb;
        }raw;
        uint16_t data;

}mynumber;
/**
 * Update Input States
 **/
void update_inputs() 
{
  /*  Remove encoders for now 
  // Encoder Flag
  for (int i = 0; i < ENC_GPIO_SIZE; i++) 
  {
    if (enc_val[i] != prev_enc_val[i]) 
    {
      enc_changed = true;
      break;
    }
  }
  */
  for (int i = 0; i < ADC_GPIO_SIZE; i++) 
  {
    adc_select_input(i);
    //mynumber.data = adc_read();
    //full = (full >> 8) & 0xFF;  // MSBs
    //adc_val[i] =  (uint8_t)(full >> 8) ;   
    //adc_val[i] =  mynumber.raw.msb ;   
    adc_val[i] = adc_read();
    //adc_val[i] = prev_adc_val[i] + i;   // Just a FUDGE value for now
    if (adc_val[i] != prev_adc_val[i])
    {
      //flashLED(3);
      adc_changed = true;
      break;
    }
  }

  // Switch Update & Flag
  for (int i = 0; i < SW_GPIO_SIZE; i++) 
  {
    if (gpio_get(SW_GPIO[i])) 
    {
      sw_val[i] = false;
    } else {
      sw_val[i] = true;
    }
    if (!sw_changed && sw_val[i] != prev_sw_val[i]) 
    {
      sw_changed = true;
    }
  }
  
  // Update LEDs if input changed while in reactive mode
  if (sw_changed && reactive_timeout_count >= REACTIVE_TIMEOUT_MAX)
    leds_changed = true;
  
}

/* Hide encoders for now
/**
 * DMA Encoder Logic For 2 Encoders
 **//*
void dma_handler() 
{
  uint i = 1;
  int interrupt_channel = 0;
  while ((i & dma_hw->ints0) == 0) 
  {
    i = i << 1;
    ++interrupt_channel;
  }
  dma_hw->ints0 = 1u << interrupt_channel;
  if (interrupt_channel < 4) 
  {
    dma_channel_set_read_addr(interrupt_channel, &pio->rxf[interrupt_channel],
                              true);
  }
}
*/

/**
 * Initialize Board Pins
 **/
void init() 
{
  // LED Pin on when connected
  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);
  gpio_put(25, 1);

  // Analog inputs
  adc_init();
  adc_gpio_init(26);
  adc_gpio_init(27);
  adc_gpio_init(28);
  adc_changed = false;
  adc_select_input(0);    

  // Set up the state machine for encoders
  /*
  pio = pio0;
  uint offset = pio_add_program(pio, &encoders_program);

  // Setup Encoders
  for (int i = 0; i < ENC_GPIO_SIZE; i++) 
  {
    enc_val[i] = 0;
    prev_enc_val[i] = 0;
    cur_enc_val[i] = 0;
    encoders_program_init(pio, i, offset, ENC_GPIO[i], ENC_DEBOUNCE);

    dma_channel_config c = dma_channel_get_default_config(i);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, i, false));

    dma_channel_configure(i, &c,
                          &enc_val[i],   // Destinatinon pointer
                          &pio->rxf[i],  // Source pointer
                          0x10,          // Number of transfers
                          true           // Start immediately
    );
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(i, true);
  }

  // Set up WS2812B
  pio_1 = pio1;
  uint offset2 = pio_add_program(pio_1, &ws2812_program);
  ws2812_program_init(pio_1, ENC_GPIO_SIZE, offset2, WS2812B_GPIO, 800000,
                      false);
  */

  // Setup Button GPIO
  for (int i = 0; i < SW_GPIO_SIZE; i++) 
  {
    sw_val[i] = false;
    prev_sw_val[i] = false;
    gpio_init(SW_GPIO[i]);
    gpio_set_function(SW_GPIO[i], GPIO_FUNC_SIO);
    gpio_set_dir(SW_GPIO[i], GPIO_IN);
    gpio_pull_up(SW_GPIO[i]);
  }

  // Setup LED GPIO
  for (int i = 0; i < LED_GPIO_SIZE; i++) 
  {
    gpio_init(LED_GPIO[i]);
    gpio_set_dir(LED_GPIO[i], GPIO_OUT);
    gpio_pull_up(LED_GPIO[i]);

    gpio_set_function(LED_GPIO[i], GPIO_FUNC_PWM);    // set them all to PWM 
    // leave examples for hard-coding debugging for now
    /*if (i == 11)
    {
      pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50,25);    // 75% at 50Mhz
    }
    else if (i == 0)
    {
      pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50,25);    // 75% at 50Mhz
      //pwm_set_wrap(pwm_gpio_to_slice_num(LED_GPIO[i]),20);   // always wrap to 255 as that is the highest value from the commands from the PC/sourc
      //pwm_set_chan_level(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]), 9);    // set all outputs fully on for the moment - adjust as needed later
    }
    else if (i == 1)
    {
      pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50,10);    // 75% at 50Mhz
      //pwm_set_wrap(pwm_gpio_to_slice_num(LED_GPIO[i]),255);   // always wrap to 255 as that is the highest value from the commands from the PC/source
      //pwm_set_chan_level(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]), 157);    // set all outputs fully on for the moment - adjust as needed later
    }
    else*/
    {
      //pwm_set_wrap(pwm_gpio_to_slice_num(LED_GPIO[i]),3);   // always wrap to 255 as that is the highest value from the commands from the PC/source
      //pwm_set_chan_level(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]), 2);    // set all outputs fully on for the moment - adjust as needed later
      pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50,100);// set all outputs fully on for the moment - adjust as needed later
    }
    pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), false);   // but start them off
  }


  // Set listener bools
  //enc_changed = false;
  sw_changed = false;
  leds_changed = false;
  adc_changed = false;

  report.adc0 = 6;     
  report.adc1 = 37;
  report.adc2 = 5;

  // Joy/KB Mode Switching
  // if (gpio_get(SW_GPIO[0])) 
  // {
    loop_mode = &joy_mode;
  // } 
  /*else {
    loop_mode = &key_mode;
  }
  */

 // flash a bit to confirm what version this is
  flashLED(3);

}

/**
 * Second Core Runnable
 **/
void core1_entry()
{
  /* Removed encoders for now
  uint32_t counter = 0;
  while (1) {
    ws2812b_update(++counter);
    sleep_ms(5);
  }
  */
}

/**
 * Main Loop Function
 **/
int main(void) 
{
  board_init();
  tusb_init();
  init();

  multicore_launch_core1(core1_entry);

  while (1) 
  {
    tud_task();  // tinyusb device task
    update_inputs();
    loop_mode();    // Joy_mode usually
    update_lights();
  }

  return 0;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t* buffer,
                               uint16_t reqlen)
{
  // TODO not Implemented
  (void)itf;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize) 
{
  (void)itf;
  //flashLED();

 // if (report_id == 2 && report_type == HID_REPORT_TYPE_OUTPUT &&
 //     buffer[0] == 2 && bufsize >= sizeof(lights_report))  // light data
  if (report_id == 2 && report_type == HID_REPORT_TYPE_OUTPUT )   // report 1 if part of "joystick"
  {

    //flashLED();
    //if ( buffer[0] == 2) //?? do we need to check for a set report type??
    {
      //flashLED();
      if (bufsize >= sizeof(lights_report))  // light data
      {

        //flashLED(1);
        size_t i = 0;
        for (i = 0; i < sizeof(lights_report); i++) 
        {
          lights_report.raw[i] = buffer[i + 2];   // increased this to +2 as seemed to be one out (maybe ignoring datatype?)
        }
        reactive_timeout_count = 0;
        leds_changed = true;
      }
      else
      {
        flashLED(bufsize);    // uhoh!
          sleep_ms(1000);      // and add a gap to show space between patterns 
        flashLED(sizeof(lights_report));    // to compare

      }

    }
  }
}
