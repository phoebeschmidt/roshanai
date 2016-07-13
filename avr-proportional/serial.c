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

#include "proportional_controller.h"

/*
#include <stdint.h>
*/


#include <avr/io.h>
#include <avr/interrupt.h>

#include "serial.h"

/*
 * Initialize the UART
 */
//#define BAUD_PRESCALE  ((F_CPU / (16UL * UART_BAUD)) - 1)
// The following prescale is close to correct for a 2MHz clock, 19400 baud, at 2X speed
#define BAUD_PRESCALE 12
void
uart_init(void)
{
  /* set baud rate */
  UBRRL = (unsigned char)BAUD_PRESCALE;   
  UBRRH = ((unsigned char)BAUD_PRESCALE) >> 8;

  UCSRB = _BV(TXEN) | _BV(RXEN); /* tx/rx enable */
  UCSRA = (1 << U2X); /* double speed mode. See note about baud, above */
  
  // URSEL - select between reading UCSRC or UBRRH  must be 1 when writing to UCSRC
  // UPM0/UPM1 - parity. Both 0 is no parity bit 
  // USBS - stop bits, inserted by transmitter. 0 - one stop bit. 1 - two stop bits
  // UCSZ0-UCSZ1 number of data bits. 1,1 is eight bits.
  // We are using asynchronous clock mode for the UART. 
  UCSRC = (1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1);  /* 8 bits per sample */
 
}

/*
 * Send character c down the UART Tx, wait until tx holding register
 * is empty.
 */
int
uart_putchar(char c, FILE *unused)
{
    /* wait unit there's space in our buffer */
    loop_until_bit_is_set(UCSRA, UDRE);

    UDR = c;

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
    
    /* turn off receiver interrupts */
    UCSRB &= ~_BV(RXCIE);
    
    /* setup - wait until line is ready, then set TE on RS-485 transceiver */
    /* (That's on PD2 ... )*/
    //loop_until_bit_is_set(UCSRA, RXC);
    //loop_until_bit_is_set(UCSRA, UDRE);
    PORTD |= _BV(RS485TRANSMIT);
    
    /* send the characters */
    while (*curChar++ != '\0') {
        uart_putchar(*curChar, NULL);
    }
    
    /* Wait until data has been sent */
    loop_until_bit_is_set(UCSRA, TXC);

    /* put RS485 chip back in read state */
    PORTD &= ~_BV(RS485TRANSMIT);
    
    /* turn on receiver interrupts */
    UCSRB |= _BV(RXCIE);

}
