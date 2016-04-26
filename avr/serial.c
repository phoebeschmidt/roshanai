/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <joerg@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Joerg Wunsch
 * ----------------------------------------------------------------------------
 *
 * Adapted 21 Jun 2006 for Flaming Lotus Girls
 *
 */

#include "poofbox.h"

/*
#include <stdint.h>
#include <stdio.h>
*/

#include <avr/io.h>
#include <avr/interrupt.h>

#include "serial.h"

/*
 * Initialize the UART
 */
#define BAUD_PRESCALE  ((F_CPU / (16UL * UART_BAUD)) - 1)
void
uart_init(void)
{
  /* set baud rate */
  UBRRL = (unsigned char)BAUD_PRESCALE;   
  UBRRH = ((unsigned char)BAUD_PRESCALE) >> 8;

  UCSRB = _BV(TXEN) | _BV(RXEN); /* tx/rx enable */
  
  // URSEL - select between reading UCSRC or UBRRH  must be 1 when writing to UCSRC
  // UPM0/UPM1 - parity. Both 0 is no parity bit 
  // USBS - stop bits, inserted by transmitter. 0 - one stop bit. 1 - two stop bits
  // UCSZ0-UCSZ1 number of data bits. 1,1 is eight bits.
  // We are using asynchronous mode for the UART.
  UCSRC = (1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1);  /* 8 bits per sample */
 
  /*  UCSRC = (3 << UCSZ0); */  /* This FUCKED it! Do not use.  */ // XXX WTF was this supposed to do?
}

/*
 * Send character c down the UART Tx, wait until tx holding register
 * is empty.
 */
int
uart_putchar(char c)
{

//while ( !( UCSRA & (1<<UDRE)) )
  loop_until_bit_is_set(UCSRA, UDRE);

  /* Set TE on RS-485 transceiver */
  /* That's PD2 ... */
  
  PORTD |= _BV(RS485TRANSMIT);

  UDR = c;

  /* Wait until data has been sent */
  loop_until_bit_is_set(UCSRA, UDRE);

  /* clear the RS-485 transmit pin */
  PORTD &= ~_BV(RS485TRANSMIT);
  /* This also needs to be implemented */

  return 0;
}


/* This turns on receive interrupts only */
void enable_serial_interrupts(void)
{
  /* Receive Complete interrupt in UART config register */
  UCSRB |= _BV(RXCIE);

  /* Now enable global interrupts */
  sei();
}

// print to serial port. Because it's good for us
void serialPrintf(const char *string) {
    const char *curChar = string;
    while (*curChar++ != '\0') {
        uart_putchar(*curChar);
    }
}
