/*
   main.c

   Copyright 2006/2015 Flaming Lotus Girls

   Program for controlling the poofer boxes

*/


#include "proportional_controller.h"   // NB - defines F_CPU, so has to go first
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdio.h>

#include "serial.h"
#include "flg-packet.h"
#include "packetReceiver.h"


/* Global variables */

/*
volatile long int    timer[NUM_RELAYS];
*/
static int         unit_address;


static void initialize(void)
{
   initialize_hw();

   unit_address = read_my_address();
   
   packetReceiver_init(unit_address);

}


static void start_processing(void)
{
   enable_serial_interrupts();
   pwm_start();
}

#ifdef DEBUG
static void display_switch_id(void)
{
    unit_address = read_my_address();
    int i;
    for (i=0; i<unit_address+1; i++) {
        set_onboardled(1);
        _delay_ms(1000);
        set_onboardled(0);
        _delay_ms(1000);
    }
}
#endif // DEBUG


#ifdef DEBUG
static int ledstate = 0;
static void toggle_onboardled(void) 
{
    set_onboardled(1-ledstate);
    ledstate = 1-ledstate;
}
#endif // DEBUG


int main(void)
{
    unsigned int adc_value = 0;
    initialize();

    set_onboardled(0);
    _delay_ms(100);

    start_processing();
   
   // Turn on onboard led as a signal that we're actually alive
      
//   char printBuf[256];
   dprintf("\n.. Starting... Initialized chip\n");
//   sprintf(printBuf, "unit address is %d\n", unit_address);
//   dprintf(printBuf);

    int iteration = 0;

    while (1) {
   
        if (pwm_use_serial()) {
            //dprintf("\nserial\n\r");
            if (packetReceiver_isPacketAvailable()) {
                _delay_ms(10); // XXX why is this here?
                packetReceiver_parsePacket();
            }
            if (have_packet_error()) {
                print_packet_errors();
                clear_packet_errors();
            }
        } else { 
            //dprintf("\ndial\n\r");
            // set pwm off of adc value
            unsigned int new_value = adc_read();
            int error = 0;
#ifdef PWM_EXTERNAL
            error = pwm_error;
#endif // PWM_EXTERNAL
            if (new_value != adc_value || error) {
#ifdef PWM_EXTERNAL
                pwm_error = 0;
#endif //PWM_EXTERNAL
                adc_value = new_value;
                set_pwm(new_value, 0);
            }
        }
    
        _delay_ms(1000);
        //sprintf(printBuf,"\nNext ... iteration %d\n", iteration++);
        //dprintf(printBuf);
        //dprintf("\nNext...\n");
        

     
        // pet watchdog
        wdt_reset();
   }


   return 0;
}
