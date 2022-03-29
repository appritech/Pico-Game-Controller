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
#define REACTIVE_TIMEOUT_MAX 500000  // Cycles before HID falls back to reactive

// Note: these are the GP numbers from the Pico
// Changed numbers to match Pigeon GPs
const uint8_t SW_GPIO[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};
// Changed numbers to match Pigeon OUTs 
const uint8_t LED_GPIO[] = {
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22
};

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


uint8_t LEDs[LED_GPIO_SIZE];
//bool FirstFlash;



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

// quick debug flash so not slowing it down but alternate every 10 ms so visible
void flipLED()
{
  LED_OnCount ++;
  if (LED_OnCount >= 200)
    LED_OnCount = 0;
  gpio_put(25,LED_OnCount>=100);     // flashe LED
  /* Test flashing PWM light as well
  if (LED_OnCount >=100)
  {
    gpio_set_function(11, GPIO_FUNC_PWM);    // stop PWM
 
    pwm_set_freq_duty(pwm_gpio_to_slice_num(11),pwm_gpio_to_channel(11),50, 100);     // ensure it is off at the end
    pwm_set_enabled(pwm_gpio_to_slice_num(11), true);   // and turn on
    //gpio_put(11,1);
  }
  else
  {
    pwm_set_freq_duty(pwm_gpio_to_slice_num(11),pwm_gpio_to_channel(11),50, 0);     // ensure it is off at the end
    pwm_set_enabled(pwm_gpio_to_slice_num(11), false);   
         gpio_set_function(11, GPIO_FUNC_SIO);    // stop PWM
 
          gpio_put(11, 0);

    //gpio_put(11,0);
    
  }
  */

}

void update_lights() 
{

  if (leds_changed) 
  {
    for (int i = 0; i < LED_GPIO_SIZE; i++) 
    {
      
        /*if (i == 11)
        {
          if (lights_report.lights.LEDs[i] > 0)
            gpio_put(LED_GPIO[i], 1);
          else
            gpio_put(LED_GPIO[i], 0);
        
        }
        
        else  */ 
        // Was having issue with not always turning off properly, but seems to be okay now
        /*
        if (lights_report.lights.LEDs[i] == 0) 
        {
          pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50, 0);// set all outputs to lowest as well
          pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), false);   // but turn them off
          gpio_set_function(LED_GPIO[i], GPIO_FUNC_SIO);    // stop PWM
 
          gpio_put(LED_GPIO[i], 0);
        }
        else if (lights_report.lights.LEDs[i] == 100) 
        {
          pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50, 100); // set to highest so will start from that when changes
          pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), false);   // but turn them off - don't need PWM at this point
          gpio_set_function(LED_GPIO[i], GPIO_FUNC_SIO);    // stop PWM
          gpio_put(LED_GPIO[i], 1);
        }
        else */
        {
          //gpio_set_function(LED_GPIO[i], GPIO_FUNC_PWM);    // start PWM again

          //pwm_set_chan_level(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]), lights_report.lights.LEDs[i]);    // set outputs to level we read
          pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50, LEDs[i] );// set all outputs fully on for the moment - adjust as needed later
          pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), true);   // and turn on

          //gpio_put(LED_GPIO[i], 1);

        }
        

    }
    leds_changed = false;
  }
}

struct report {
  uint16_t buttons;
  uint8_t adc0;
  uint8_t adc1;
  uint8_t adc2;     // if report changes, need to change Unity reader
} report;

void inputs_mode() 
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




    if (send_report) 
    {
      //flashLED();
      tud_hid_n_report(0x00, REPORT_ID_INPUTS, &report, sizeof(report));
      report_timer_count = 0;   // we sent, so reset the wait
      flipLED();    // don't use the pausing version here

    }
  }
}


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
  for (int i = 0; i < ADC_GPIO_SIZE; i++) 
  {
    adc_select_input(i);
    //mynumber.data = adc_read();
    //full = (full >> 8) & 0xFF;  // MSBs
    //adc_val[i] =  (uint8_t)(full >> 8) ;   
    //adc_val[i] =  mynumber.raw.msb ;   
    // Unity will normalize the incominb values, so just read it here
    adc_val[i] = adc_read();
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

    //if (i < 11)    //*** set all but last one for now
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
      //pwm_set_freq_duty(pwm_gpio_to_slice_num(LED_GPIO[i]), pwm_gpio_to_channel(LED_GPIO[i]),50,100);// set all outputs fully on for the moment - adjust as needed later
    }
    //pwm_set_enabled(pwm_gpio_to_slice_num(LED_GPIO[i]), false);   // but start them off
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
    loop_mode = &inputs_mode;     // only mode we are running for now
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
   // Consider reporting debug information from this core, or moving the light flashing here to separate it from any input delays
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

  // Looks like this is never being called - but keep the flash in case it happens
  flashLED(4);

  return 0;

  /*
  if (report_id == 2 && report_type == HID_REPORT_TYPE_OUTPUT )   // try & report debug info back out this way
  {
    flashLED(4);
     for (int i = 0; i < 40; i++)
    {
      buffer[i] = 0;      // just clear it out for the moment
    }
    uint16_t returnLength = 40;
    return returnLength;
  }
  */

}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize) 
{
  (void)itf;
  
  /*
  if (FirstFlash == false)
  {
    flashLED(4);    // something to show we got summat
    flashLED(report_id);   // should be 2
    flashLED(report_type);  // should be 2
    FirstFlash = true;
  }
  */

 
  if (report_id == 2 && report_type == HID_REPORT_TYPE_OUTPUT )   // 
  {

    //flashLED();
    if (bufsize >= sizeof(LEDs))  // light data
    {

      //flashLED(2);
      size_t i = 0;
      for (i = 0; i < sizeof(LEDs); i++) 
      {
        LEDs[i] = buffer[i + 2];   // increased this by 2 to ignore report id & datatype
      }
      reactive_timeout_count = 0;
      leds_changed = true;
    }
    else
    {
      flashLED(bufsize);    // uhoh!
      sleep_ms(1000);      // and add a gap to show space between patterns 
      flashLED(sizeof(LEDs));    // to compare

    }

  }
}
