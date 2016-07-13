/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <joerg@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Joerg Wunsch
 * ----------------------------------------------------------------------------
 */
/*
 * ----------------------------------------------------------------------------
 * Updated to handle larger devices having 16-bit addresses
 *                                                 (2007-09-05) Ruwan Jayanetti
 * ----------------------------------------------------------------------------
 */
 
 /* Modified by FLG for Pulse, 2016, for talking with the PCA9685.
    See http://www.nongnu.org/avr-libc/user-manual/group__twi__demo.html
    -- CSW */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <avr/io.h>
#include <util/twi.h>

#include "proportional_controller.h" // for things like CPU speed XXX
#include "serial.h" // for things like dprintf

//#define DEBUG 1


/*
 * Compatibility defines.  This should work on ATmega8, ATmega16,
 * ATmega163, ATmega323 and ATmega128 (IOW: on all devices that
 * provide a builtin TWI interface).
 *
 * On the 128, it defaults to USART 1.
 */
 
#ifndef UCSRB
# ifdef UCSR1A		/* ATmega128 */
#  define UCSRA UCSR1A
#  define UCSRB UCSR1B
#  define UBRR UBRR1L
#  define UDR UDR1
# else /* ATmega8 */
#  define UCSRA USR
#  define UCSRB UCR
# endif
#endif
#ifndef UBRR
#  define UBRR UBRRL
#endif


/* TWI addressing for PC9685: 
 * 
 * ----------------------------------------
 * | 1 | a5 | a4 | a3 | a2 | a1 | a0 | rw |
 * ----------------------------------------
 * 
 * where a0 - a5 are set via external hardware
 * (for FLG, I'm tying them all low) 
 * rw determines whether we are trying to read (1) 
 * or write (0)
 */



/*
 * Maximal number of iterations to wait for a device to respond for a
 * selection.  Should be large enough to allow for a pending write to
 * complete, but low enough to properly abort an infinite loop in case
 * a slave is broken or not present at all.  With 100 kHz TWI clock,
 * transfering the start condition and SLA+R/W packet takes about 10
 * µs.  The longest write period is supposed to not exceed ~ 10 ms.
 * Thus, normal operation should not require more than 100 iterations
 * to get the device to respond to a selection.
 */
#define MAX_ITER    200

/*
 * Saved TWI status register, for error messages only.  We need to
 * save it in a variable, since the datasheet only guarantees the TWSR
 * register to have valid contents while the TWINT bit in TWCR is set.
 */

static uint8_t twst;

/*
 * Do all the startup-time peripheral initializations: UART (for our
 * debug/test output), and TWI clock.
 */
void twi_init(void)
{

#if 0 // UART set up elsewhere
#if F_CPU <= 1000000UL
    /*
    * Note [4]
    * Slow system clock, double Baud rate to improve rate error.
    */
    UCSRA = _BV(U2X);  
    UBRR = (F_CPU / (8 * 9600UL)) - 1; /* 9600 Bd */
#else
    UBRR = (F_CPU / (16 * 9600UL)) - 1; /* 9600 Bd */
#endif
    UCSRB = _BV(TXEN);		/* tx enable */
#endif //0 UART

  /* initialize TWI clock: 100 kHz clock, TWPS = 0 => prescaler = 1 */
#if defined(TWPS0)
    /* has prescaler (mega128 & newer) */
    TWSR = 0;
#endif

#if F_CPU < 3600000UL
    TWBR = 10;                /* smallest TWBR value, see note [5] */
#else
    TWBR = (F_CPU / 100000UL - 16) / 2;
#endif
}

/*
 * Note [6]
 * Send character c down the UART Tx, wait until tx holding register
 * is empty.
 */
#if 0
int
uart_putchar(char c, FILE *unused)
{

  if (c == '\n')
    uart_putchar('\r', 0);
  loop_until_bit_is_set(UCSRA, UDRE);
  UDR = c;
  return 0;
}
#endif //0

/*
 * Note [7]
 *
 * Read "len" bytes from EEPROM starting at "eeaddr" into "buf".
 *
 * This requires two bus cycles: during the first cycle, the device
 * will be selected (master transmitter mode), and the address
 * transfered.
 * Address bits exceeding 256 are transfered in the
 * E2/E1/E0 bits (subaddress bits) of the device selector.
 * Address is sent in two dedicated 8 bit transfers
 * for 16 bit address devices (larger EEPROM devices)
 *
 * The second bus cycle will reselect the device (repeated start
 * condition, going into master receiver mode), and transfer the data
 * from the device to the TWI master.  Multiple bytes can be
 * transfered by ACKing the client's transfer.  The last transfer will
 * be NACKed, which the client will take as an indication to not
 * initiate further transfers.
 */
#if 0
int
ee24xx_read_bytes(uint16_t eeaddr, int len, uint8_t *buf)
{
  uint8_t sla, twcr, n = 0;
  int rv = 0;

#ifndef WORD_ADDRESS_16BIT
  /* patch high bits of EEPROM address into SLA */
  sla = TWI_SLA_24CXX | (((eeaddr >> 8) & 0x07) << 1);
#else
  /* 16-bit address devices need only TWI Device Address */
  sla = TWI_SLA_24CXX;
#endif

  /*
   * Note [8]
   * First cycle: master transmitter mode
   */
  restart:
  if (n++ >= MAX_ITER)
    return -1;
  begin:

  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* send start condition */
  while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
  switch ((twst = TW_STATUS))
    {
    case TW_REP_START:		/* OK, but should not happen */
    case TW_START:
      break;

    case TW_MT_ARB_LOST:	/* Note [9] */
      goto begin;

    default:
      return -1;		/* error: not in start condition */
				/* NB: do /not/ send stop condition */
    }

  /* Note [10] */
  /* send SLA+W */
  TWDR = sla | TW_WRITE;
  TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
  while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
  switch ((twst = TW_STATUS))
    {
    case TW_MT_SLA_ACK:
      break;

    case TW_MT_SLA_NACK:	/* nack during select: device busy writing */
				/* Note [11] */
      goto restart;

    case TW_MT_ARB_LOST:	/* re-arbitrate */
      goto begin;

    default:
      goto error;		/* must send stop condition */
    }

#ifdef WORD_ADDRESS_16BIT
  TWDR = (eeaddr >> 8);		/* 16-bit word address device, send high 8 bits of addr */
  TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
  while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
  switch ((twst = TW_STATUS))
    {
    case TW_MT_DATA_ACK:
      break;

    case TW_MT_DATA_NACK:
      goto quit;

    case TW_MT_ARB_LOST:
      goto begin;

    default:
      goto error;		/* must send stop condition */
    }
#endif

  TWDR = eeaddr;		/* low 8 bits of addr */
  TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
  while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
  switch ((twst = TW_STATUS))
    {
    case TW_MT_DATA_ACK:
      break;

    case TW_MT_DATA_NACK:
      goto quit;

    case TW_MT_ARB_LOST:
      goto begin;

    default:
      goto error;		/* must send stop condition */
    }

  /*
   * Note [12]
   * Next cycle(s): master receiver mode
   */
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* send (rep.) start condition */
  while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
  switch ((twst = TW_STATUS))
    {
    case TW_START:		/* OK, but should not happen */
    case TW_REP_START:
      break;

    case TW_MT_ARB_LOST:
      goto begin;

    default:
      goto error;
    }

  /* send SLA+R */
  TWDR = sla | TW_READ;
  TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
  while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
  switch ((twst = TW_STATUS))
    {
    case TW_MR_SLA_ACK:
      break;

    case TW_MR_SLA_NACK:
      goto quit;

    case TW_MR_ARB_LOST:
      goto begin;

    default:
      goto error;
    }

  for (twcr = _BV(TWINT) | _BV(TWEN) | _BV(TWEA) /* Note [13] */;
       len > 0;
       len--)
    {
      if (len == 1)
	twcr = _BV(TWINT) | _BV(TWEN); /* send NAK this time */
      TWCR = twcr;		/* clear int to start transmission */
      while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
      switch ((twst = TW_STATUS))
	{
	case TW_MR_DATA_NACK:
	  len = 0;		/* force end of loop */
	  /* FALLTHROUGH */
	case TW_MR_DATA_ACK:
	  *buf++ = TWDR;
	  rv++;
	  if(twst == TW_MR_DATA_NACK) goto quit;
	  break;

	default:
	  goto error;
	}
    }
  quit:
  /* Note [14] */
  TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN); /* send stop condition */

  return rv;

  error:
  rv = -1;
  goto quit;
}
#endif // 0

/*
 * Write "len" bytes into device at addr.
 * 
 * For PCA, the first byte in the buffer is the register address,
 * and the second byte is the data. All subsequent bytes will be written
 * to the same register unless the 'autoincrement' flag in the PCA's MODE1
 * register is set, in which case subsequent bytes will be written to
 * the next registers. 
 *
 * This is a bit simpler than the previous function since both, the
 * address and the data bytes will be transfered in master transmitter
 * mode, thus no reselection of the device is necessary.  
 
 
 * 
 * However, the
 * EEPROMs are only capable of writing one "page" simultaneously, so
 * care must be taken to not cross a page boundary within one write
 * cycle.  The amount of data one page consists of varies from
 * manufacturer to manufacturer: some vendors only use 8-byte pages
 * for the smaller devices, and 16-byte pages for the larger devices,
 * while other vendors generally use 16-byte pages.  We thus use the
 * smallest common denominator of 8 bytes per page, declared by the
 * macro PAGE_SIZE above.
 *
 * The function simply returns after writing one page, returning the
 * actual number of data byte written.  It is up to the caller to
 * re-invoke it in order to write further data.
 */
#define TIMEOUT_COUNT 1000

int
pca9685_write(uint8_t addr, int len, uint8_t *buf)
{
    uint8_t n = 0;
    int rv = 0;
  
    restart:
    if (n++ >= MAX_ITER)
        return -1;
    begin:

    /* Note [15] */

    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* send start condition */
    int stopcount = 0;
    while ((TWCR & _BV(TWINT)) == 0) { /* wait for transmission */
        if (stopcount++ >= TIMEOUT_COUNT) {
            dprintf("\nBus failure - could not send START\n");
            set_onboardled(1);
            return -1;
        }
    }

    switch ((twst = TW_STATUS))
    {
        case TW_REP_START:            /* OK, but should not happen */
        case TW_START:
            break;

        case TW_MT_ARB_LOST:
            goto begin;

        default:
            return -1;                /* error: not in start condition */
                                      /* NB: do /not/ send stop condition */
    }

    /* send SLA+W */
    stopcount = 0;
    TWDR = addr | TW_WRITE;
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while ((TWCR & _BV(TWINT)) == 0){ /* wait for transmission */
        if (stopcount++ >= TIMEOUT_COUNT) {
            dprintf("\nBus failure - could not send SLA+W\n");
            set_onboardled(1);
            return -1;
        }
    }
    switch ((twst = TW_STATUS))
    {
        case TW_MT_SLA_ACK:
            break;

        case TW_MT_SLA_NACK:     /* nack during select: device busy writing */
            goto restart;

        case TW_MT_ARB_LOST:     /* re-arbitrate */
            goto begin;

        default:
            goto error;          /* must send stop condition */
    }

    
    for (; len > 0; len--) {
        stopcount = 0;
        TWDR = *buf++;
        TWCR = _BV(TWINT) | _BV(TWEN); /* start transmission */
        while ((TWCR & _BV(TWINT)) == 0) { /* wait for transmission */
            if (stopcount++ >= TIMEOUT_COUNT) {
                dprintf("\nBus failure - could not send data\n");
                set_onboardled(1);
                return -1;
            }
        }
        switch ((twst = TW_STATUS))
        {
            case TW_MT_DATA_NACK:
              goto error;   /* device write protected -- Note [16] */

            case TW_MT_DATA_ACK:
              rv++;
              break;

            default:
              goto error;
        }
    }

    quit:
    TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN); /* send stop condition */

    return rv;

    error:
    rv = -1;
    goto quit;
}


void
error(void)
{

  printf("error: TWI status %#x\n", twst);
  exit(0);
}

//FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
//stdout = &mystdout;

