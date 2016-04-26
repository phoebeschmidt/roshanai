/*
   poofbox.c

   Copyright 2006/2016 Flaming Lotus Girls

   This file is for code unique to the poofer control box hardware

*/

#include <avr/io.h>
#include <avr/interrupt.h>

#include "poofbox.h"
#include "serial.h"

unsigned char relayIdx[NUM_RELAYS]  = {0,1,2,3,4,5,0,1};
char relayPort[NUM_RELAYS] = {'C','C','C','C','C','C','B','B'};


void ioport_init(void)
{
  /* The first 6 relays are controlled by PORTC, same as the serpent
     The last 2 are controlled by PORTB */

  /* Port B has 3 outputs */
  /* 3,4,5,6 and 7 are ISP header and XTAL osc, so don't touch them! */
  DDRB = (1 << RELAY6) | (1 << RELAY7) | (1 << ONBOARDLED);
  
  /* Port C has relay outputs only */
  DDRC = (1 << RELAY0) |
         (1 << RELAY1) |
         (1 << RELAY2) |
         (1 << RELAY3) |
         (1 << RELAY4) |
         (1 << RELAY5);

  /* Port D uses 0 and 1 for the serial data, 2 is the TE output,
     and the others are dip-switch inputs */
  DDRD = (1 << RS485TRANSMIT);

  /* enable internal pullups on dipswitch ports */
  PORTD = SWITCH_MASK << SWITCH_SHIFT;
}


void initialize_hw(void)
{
  /* Clear all interrupts */
  cli();

  /* Initialize the I/O ports */
  ioport_init();

  /* Initialize the UART */
  uart_init();
}



/* Read the port which has the switch connected, and ignore non-switch bits */
int read_dip_switch(void)
{
  return ((PIND >> SWITCH_SHIFT) & SWITCH_MASK);
}

int read_my_address(void)
{
   return read_dip_switch();
}


void set_onboardled(int value)
{
  if (value)
    PORTB |= (1 << ONBOARDLED);
  else
    PORTB &= ~(1 << ONBOARDLED);
}

void set_relay(int relaynum, int value) 
{
   if (relaynum > NUM_RELAYS) 
      return;

   
   if (relayPort[relaynum] == 'C') {
      if (value) {
         PORTC |= (1<<relayIdx[relaynum]);
      } else {
         PORTC &= ~(1<<relayIdx[relaynum]);
      }
   } else {
      if (value) {
         PORTB |= (1<<relayIdx[relaynum]);
      } else {
         PORTB &= ~(1<<relayIdx[relaynum]);
      }
   }   
}


