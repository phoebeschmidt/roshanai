/*
   poofbox.h

   Copyright 2006/2016 Flaming Lotus Girls

   Constants specific for the poofer control boxes used for Pulse.
   
   Note that these are the 8-relay driver boards that were also used for 
   Mutopia (um. I think.) - CSW

*/

#define   F_CPU      2000000UL
/*
#define   F_CPU      7372800UL  // clockspeed of crystal
*/

//#define PWM_EXTERNAL

#define   UART_BAUD  19200

/* This about 24000 per second:  it's in units of "loop iterations"  
    NB - this is only true if you're using the 1MHz internal clock. If you're
    using the crystal, your timing will be very different*/
//#define   RELAY_TIMEOUT   (7*12000)UL  // XXX - I'm getting an integer overflow when I multiply be 7 - are we really 16 bits here?
#define   RELAY_TIMEOUT   84000UL  // XXX - I'm getting an integer overflow when I multiply be 7 - are we really 16 bits here?

/* 1 PWM at 1MHz on the board. If I can get the external PWM to work, we'll have 6 */ 
#define NUM_VALVES 1

/* Our port connections */

/* Port B */
#define   RELAY6     PB0  // XXX these are the outputs for timer 1
#define   RELAY7     PB1
#define   ONBOARDLED PB2

/* Port C */
/* Relay indices are 0-based */
/* Outlet numbers in the external world are 1-based; see parse_packet() in flg-packet.c */
/*
#define   RELAY0     PC0
#define   RELAY1     PC1
#define   RELAY2     PC2
#define   RELAY3     PC3
#define   RELAY4     PC4
#define   RELAY5     PC5
*/

/* Port D */
/* Note switch indices are 1-based cause the switch is labeled that way on the PC board! */
/* Note that the switch on the board has 8 different subswitches. The other three can be used
   to default the SPI lines to high or low - CSW, 4/16 */
#define   RS485TRANSMIT   PD2
#define   SWITCH1         PD3  // XXX - being used to select between dial and serial for pwm input
#define   SWITCH2         PD4
#define   SWITCH3         PD5
#define   SWITCH4         PD6
#define   SWITCH5         PD7


/* For address read from DIP switch */
#define   SWITCH_MASK     0x1f
#define   SWITCH_SHIFT    3

/* error flag */
#ifdef PWM_EXTERNAL
extern int pwm_error; 
#endif 

void initialize_hw(void);

int read_my_address(void);


void set_onboardled(int value);
void set_valve(int valve, unsigned int value);
void set_pwm(unsigned int value, unsigned int channel);
unsigned int adc_read(void);
void pwm_start(void);
int pwm_use_serial(void);

