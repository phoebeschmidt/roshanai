/*
   flg-packet.c

   Copyright 2006/2016 Flaming Lotus Girls
   Modified for Pulse by csw, 2016

*/


// In this version I'm adding control for a proportional valve - the last two 
// bytes are the hex values for the amount to open the valve, from 0 to 255

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include "flg-packet.h"
#include "proportional_controller.h"
#include "serial.h"
#include <util/delay.h>


static int packet_available = 0;
static int packet_state;
static int current_channel;
static tpacket packet;
static int unit_address;

static void clear_packet(tpacket *p);

static void buffer_error(const char *errString);



void packetReceiver_init(int address)
{
    packet_available = 0;
    packet_state = PACKET_STATE_IDLE;
    current_channel = 0;
    clear_packet(&packet);
    unit_address = address;
}

int packetReceiver_isPacketAvailable(void) 
{
    return packet_available;
}
/*
   UART receive interrupt:  collect packet
*/


ISR(USART_RXC_vect)
{
    char c;
    int channel;

    c = UDR;

    // If the previous packet has not been acted on, don't overwrite it
    if (packet_available) {
//        buffer_error("\nPrevious packet not parsed, ignoring new data\n");
        return;
    }

    /* Ignore a space character, with a nod to Lee */
    if (c == ' ')
        return;

    /* Even if we're in the middle of a packet, state is reset by receiving
      a packet start character */
    if (c == PACKET_CHAR_WRITE || c == PACKET_CHAR_READ) {
        packet_state = PACKET_STATE_IDLE;
        clear_packet(&packet);
    }


    switch (packet_state) {
    case PACKET_STATE_IDLE:
        if (c == PACKET_CHAR_WRITE || c == PACKET_CHAR_READ) {
            packet.command = c;
            packet_state = PACKET_STATE_UNIT1_WAIT;
            set_onboardled(1);
        } else {
            char buffer[64];
            sprintf(buffer, "\nUnexpected character, 0x%x, %d %c\n", c, c, c);
            buffer_error(buffer);
            /* Here, we've received something other than a packet start while idling.
               This is a kind of error, too, but just shows there's garbage on the line */
            ;
        }
        break;


    case PACKET_STATE_UNIT1_WAIT:
        packet.controller_id[0] = c;
        packet_state = PACKET_STATE_UNIT2_WAIT;
        break;

    case PACKET_STATE_UNIT2_WAIT:
        packet.controller_id[1] = c;
        packet_state = PACKET_STATE_ADDR_WAIT;
        break;

    case PACKET_STATE_ADDR_WAIT:
        channel = c - '0';  // find channel as an index rather than ascii. Note that the protocol is 1 based rather than 0 based, although 0 is a special case 'all off' command
        if (channel < 0 || channel > MAX_CHANNELS) { 
            buffer_error("Unknown channel, ignoring\n");
        } else {
            current_channel = channel;
            packet_state = PACKET_STATE_DATA1_WAIT;
        }
        break;

    case PACKET_STATE_DATA1_WAIT:  // two bytes here..
        if (current_channel == 0) { // special case, allows 'all off' command
            if (c == '0') {
                packet.all_off = 1;
                packet_state = PACKET_STATE_TAIL_WAIT;
            } // ignore anything other than a '0'... in this case. there is no 'all on' command
        } else if ((c >= '0' && c <= '9') || 
                     (c >= 'a' && c <= 'f') || 
                     (c >= 'A' && c <= 'F')) {
            packet.data[current_channel - 1][0] = c;
            packet_state = PACKET_STATE_DATA2_WAIT;
        } else {
            buffer_error("Packet payload bad, ignoring\n");
        }

        break;

    case PACKET_STATE_DATA2_WAIT:  // two bytes here..
        if ((c >= '0' && c <= '9') || 
                     (c >= 'a' && c <= 'f') || 
                     (c >= 'A' && c <= 'F')) {
            packet.data[current_channel - 1][1] = c;
            packet_state = PACKET_STATE_TAIL_WAIT;
        } else {
            buffer_error("Packet payload bad, ignoring\n");
        }
        break;

    case PACKET_STATE_TAIL_WAIT:  // this state is actually a wait for a separator *or* a wait for a tail.
        if (c == PACKET_CHAR_TAIL) {
            //buffer_error("OOOH! have packet\n");
            packet_available = 1;
            packet_state = PACKET_STATE_IDLE;
        } else if (c == CHANNEL_SEPARATOR) {
            packet_state = PACKET_STATE_ADDR_WAIT;
        } else {
            /* This is an error condition:  a tail character was expected, but not received,
                so the character is discarded */
            ;
        }

        break;

    default:
        /* this should never happen... we should always be inside our happy state machine */
        /* discard c! */
        /* Always go to the idle state, and ensure there's no packet available */
        packet_state = PACKET_STATE_IDLE;
        packet_available = 0;
        break;
}

   /* Clear the LED if we go to idle state */
   if (packet_state == PACKET_STATE_IDLE)
      set_onboardled(0);

}



static int hex2int(char letter)
{
    if (letter >= '0' && letter <= '9')
        return (letter - '0');
    else if (letter >= 'a' && letter <= 'f')
        return ((letter - 'a') + 10);
    else if (letter >= 'A' && letter <= 'F')
        return ((letter - 'A') + 10);

    return 999;         /* ha ha ha, this is bogus, but safe for this application */
}



/*
    Look at a packet and do something with it
    This is the part which defines the behavior that each packet can create
*/

void packetReceiver_parsePacket(void)
{
    int unit1, unit2;
    int unit = 0;
    char buf[256];

    unit1 = hex2int(packet.controller_id[0]);
    unit2 = hex2int(packet.controller_id[1]);

    unit = (unit1 << 4) + unit2;
//    sprintf(buf, "\nPP - id %d\n", unit);
//    dprintf("\nhi\n");
//    _delay_ms(10);

    /* Special case for broadcast all off */  // XXX I've just lost this!!! This will not work!!!
    if (unit == 0 && packet.all_off == 1) {
        int i;
        for (i=0; i < NUM_VALVES; i++ ){
            set_valve(i, 0);
        }
    } else if (unit == unit_address) {
        //dprintf("\n2\n");  
        _delay_ms(10);
        /* handle channel commands in the packet */
        int valveNum;
        for (valveNum=0; valveNum < NUM_VALVES; valveNum++) {
            unsigned int valveValue = (hex2int(packet.data[valveNum][0]) << 4) + hex2int(packet.data[valveNum][1]); 
            sprintf(buf, "v %d : %d\n", valveNum, (valveValue*100)/255);
            dprintf(buf);
            set_valve(valveNum, valveValue);
        }   
     }

    /* We've looked at the packet and either used it or ignored it */
    clear_packet(&packet);

}

#define ERRBUF_SIZE 128
static char errorBuf[ERRBUF_SIZE];
static unsigned int errorOffset = 0;

static void buffer_error(const char *errString) {
    int stringlen = strlen(errString);
    if ((stringlen + errorOffset) < (ERRBUF_SIZE - 1)) {
        sprintf(errorBuf + errorOffset, errString);
        errorOffset += stringlen;
    }

//    packet_state = PACKET_STATE_TAIL_WAIT;
    return;
}

void clear_packet_errors(void)
{
    errorBuf[0] = '\0';
    errorOffset = 0;
}

void print_packet_errors(void)
{
    dprintf(errorBuf);
}

int have_packet_error(void)
{
    return errorOffset > 0;
}


static void clear_packet(tpacket *p)
{
    p->command = '\0';
    p->controller_id[0] = '\0';
    p->controller_id[1] = '\0';
    memset(&p->data[0], CHANNEL_COMMAND_NOP, MAX_CHANNELS*2);
    packet_available = 0;
    p->all_off = 0;
}

