/*
   proportional_controller.c

   Copyright 2006/2016 Flaming Lotus Girls

   This file is for code unique to the proportional controller hardware

*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdlib.h>

#include "proportional_controller.h"
#include "serial.h"
#include "twi.h"
#include "pca9685.h"
#include <util/delay.h>

#ifdef PWM_EXTERNAL
int pwm_error = 0;   // XXX why do I have this ?
static int pwm_write_register(unsigned char address, unsigned char data);
#endif //PWM_EXTERNAL

static void watchdog_init(void)
{
    wdt_reset();
    
    /* tell system that watchdog settings are going to change */
    WDTCR |= (1<<WDCE) | (1<<WDE);
    
    /* change watchdog settings - enable watchdog, set ~0.5s timeout */
    WDTCR = (1<<WDE) | 1<<WDP2 | (1<<WDP0);
}

static void ioport_init(void)
{
  
  /* Port B has 3 outputs */
  /* 3,4,5,6 and 7 are ISP header and XTAL osc, so don't touch them! */  // XXX will this be true?
  DDRB |= (1 << ONBOARDLED);
  
  /* Port D uses 0 and 1 for the serial data, 2 is the TE output,
     and the others are dip-switch inputs */
  DDRD |= (1 << RS485TRANSMIT);

  /* enable internal pullups on dipswitch ports */
  PORTD = SWITCH_MASK << SWITCH_SHIFT;
}

static void adc_init(void)
{
    // Enable AD converter, set scale factor 32 (final clock rate must be between 50 and 200 KHz)
    // Starting with clock rate of 4 Mhz. 
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS0);
    
    // set AD input to PC0 (pin 23) 
    ADMUX=0x00;
}

// this is the non-freerunning version of the converter. Good enough for right now though.
// Returns a value between 0 and 1024.
unsigned int adc_read(void)
{
    ADCSRA |= (1<<ADSC); // start data acquisition
    while (ADCSRA & (1<<ADSC)); // wait for data
    return ADCW; 
}

#ifdef PWM_EXTERNAL
static int pwm_write(unsigned char *data, int len) 
{
    return pca9685_write(TWI_SLA_PCA9685|TWI_PCA9685_WRITE, len, data);
    // XXX print ret? What happened??
}

static int pwm_write_register(unsigned char reg_addr, unsigned char value)
{
    int bytes_written = 0;
    unsigned char data[2];
    data[0] = reg_addr;
    data[1] = value;
    bytes_written = pwm_write(data, 2);
    if (bytes_written != 2) {
        return -1;
    }
    return 0;
}
#endif // PWM_EXTERNAL

void pwm_start(void)
{
#ifdef PWM_EXTERNAL
    _delay_ms(1);
    pwm_write_register(PCA9685_MODE1, 0x01);  /* set mode 1 - mostly default values, but takes it out of low power mode */
    _delay_ms(1);
    pwm_write_register(PCA9685_MODE1, 0xa1);  /* enable auto increment and restart */
    pwm_write_register(PCA9685_MODE2, 0x00);  /* enable open drain */
    pwm_write_register(PCA9685_PRESCALE, 0x07); // XXX 800Hz
#else    
#endif //PWM_EXTERNAL
}

static void pwm_init(void)
{
#ifdef PWM_EXTERNAL
#else    
    DDRB |= (1 << DDB1);     // output on PB1

    TCCR1A |= (1 << COM1A1); // non-inverting mode

    TCCR1A |= (1 << WGM11) | (1 << WGM10); // set 10bit phase corrected PWM Mode

    TCCR1B |= (1 << CS10); // set prescaler to 64, starts PWM. We believe we're using the 2MHz internal clock
#endif //PWM_EXTERNAL
}


// value is 10-bits
void set_pwm(unsigned int value, unsigned int channel)
{

#ifdef PWM_EXTERNAL
    unsigned char data[5];
#endif // PWM EXTERNAL
    if (channel >= NUM_VALVES) {
        return;
    }
    
    if (value > 1023) value = 1023;
    if (value > 512) {
        set_onboardled(1);
    } else {
        set_onboardled(0);
    }
    
    
#ifdef PWM_EXTERNAL
    if (channel >= NUM_VALVES) {
        return;
    }
    uint16_t delay = (uint16_t)(rand()%4096); 
    int onTime = ((4096 * value) >> 10);
    int offTime = delay + onTime;
    if (offTime >= 4096) offTime -= 4096;
    
    data[0] = PCA9685_CHANNEL0 + 4*channel;
    data[1] = (unsigned char)(delay & 0x00FF);
    data[2] = (unsigned char)(delay >> 8);
    data[3] = (unsigned char)(offTime & 0x00FF);
    data[4] = (unsigned char)(offTime>>8);
    
    if (pwm_write(data, 5) != 5) {
        pwm_error = 1;
    }
#else
    OCR1A = value; //(we're using the 10 bit timer)
#endif //PWM_EXTERNAL
}

void initialize_hw(void)
{
    /* Clear all interrupts */
    cli();

    /* Initialize the I/O ports */
    ioport_init();
    
    /* Initialize the UART */
    uart_init();

    /* initialize ADC */
    adc_init();

#ifdef PWM_EXTERNAL    
    /* initialize TWI */
    twi_init();
#endif // PWM_EXTERNAL

    /* initialize PCM */
    pwm_init();
    
    /* initialize watchdog */
    //watchdog_init();

}



/* Read the port which has the switch connected, and ignore non-switch bits */
int read_dip_switch(void)
{
  return ((PIND >> SWITCH_SHIFT) & SWITCH_MASK);
}

int pwm_use_serial(void)
{
    return PIND & (1<<PD3);
}

int read_my_address(void)
{
    // at the moment, it's just 0
    return 0;
   //return read_dip_switch();
}

void set_onboardled(int value)
{
  if (value)
    PORTB |= (1 << ONBOARDLED);
  else
    PORTB &= ~(1 << ONBOARDLED);
}

// Value coming in here is 8 bits.
void set_valve(int valveNum, unsigned int value) 
{
    set_pwm(value << 2, valveNum); // translate to 10 bits
/*
    if (valveNum >= NUM_VALVES || valveNum < 0) 
      return;
    
    if (valveValue[valveNum] != value) {
        valveValue[valveNum] = value;
        set_pcm(value);  // maybe one day we'll have more valves...
    }
*/
/*   
   if (relayPort[valveNum] == 'C') {
      if (value) {
         PORTC |= (1<<relayIdx[valveNum]);
      } else {
         PORTC &= ~(1<<relayIdx[valveNum]);
      }
   } else {
      if (value) {
         PORTB |= (1<<relayIdx[valveNum]);
      } else {
         PORTB &= ~(1<<relayIdx[valveNum]);
      }
   }   
*/
}


