/*
   main.c

   Copyright 2006/2015 Flaming Lotus Girls

   Program for controlling the poofer boxes

*/


#include "poofbox.h"
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

#include "serial.h"
#include "flg-packet.h"
#include "packetReceiver.h"

/* Global variables */
/*
int         packet_available;
int         packet_state;
tpacket     packet;
int         received_char;
*/

volatile long int    timer[NUM_RELAYS];
int         unit_address;

void initialize(void)
{
   int i;

   initialize_hw();

   unit_address = read_my_address();
   
   packetReceiver_init(unit_address, &timer[0]);

   for (i = 0; i < NUM_RELAYS; i++)
     timer[i] = 0;
}


void start_processing(void)
{
   enable_serial_interrupts();
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

void service_timers(void)
{
  
  int i;
  
#ifdef DEBUG
  static long int  debugTimer;
  if (debugTimer > 0) {
     debugTimer--;
  } else {
     display_switch_id();
//     toggle_onboardled();
     debugTimer = 90000;
  }
#endif // DEBUG

  for (i = 0; i < NUM_RELAYS; i++) {

    /* Turn off relay if timer has reached 0 */
    if (timer[i] <= 0) {
       set_relay(i, 0);
    /* Otherwise, count down */
    } else { 
       timer[i]--;
    }
  }
}


int main(void)
{
   initialize();

   start_processing();
   
   // Turn on onboard led as a signal that we're actually alive
   set_onboardled(1);
      
   char printBuf[256];
   dprintf("\n.. Starting... Initialized chip\n");
   sprintf(printBuf, "unit address is %d\n", unit_address);
   dprintf(printBuf);

   while (1) {

     if (packetReceiver_isPacketAvailable()) {
         packetReceiver_parsePacket();
     }

     service_timers();
   }

   return 0;
}
