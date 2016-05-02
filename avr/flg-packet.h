/*
   flg-packet.h

   Copyright 2006/2016 Flaming Lotus Girls
   
   extended for Pulse by csw, 2016

*/

// FLG Packet definitions  

#ifndef FLG_PACKET_DEF

#define  PACKET_CHAR_WRITE   '!'
#define  PACKET_CHAR_READ    '?'
#define  PACKET_CHAR_RESPOND '^'
#define  PACKET_CHAR_TAIL    '.'


#define  PACKET_STATE_IDLE       0
#define  PACKET_STATE_HEAD_OK    1
#define  PACKET_STATE_UNIT2_WAIT 2
#define  PACKET_STATE_ADDR_WAIT  3
#define  PACKET_STATE_DATA_WAIT  4
#define  PACKET_STATE_TAIL_WAIT  5


/* These don't make sense yet:  they need to be character pairs */
/* Specifically, NOP would be '0' and '0', and bcast 'f' and 'f' */
/* These are unit numbers */
#define  NOP                   0x00
#define  BROADCAST             0xff
#define  MAX_CHANNELS                   8   // maximum number of channels per controller, currently 8
#define  CHANNEL_COMMAND_ON            '1'  // turn specified channel on
#define  CHANNEL_COMMAND_OFF           '0'  // turn specified channel off
#define  CHANNEL_COMMAND_NOP           '3'  // do nothing to specified channel
#define  CHANNEL_SEPARATOR             '~'  // separator between channel commands 

// Over the wire, a packet is defined as:
// - command associated with packet (1 byte)
// - controller id the packet is intended for (2 bytes)
// - One or more of the following:
// -   channel id (1 byte)
// -   specific command for channel (1 byte)
// -   separator (may be omitted in final channel command)
// - terminator character (PACKET_CHAR_TAIL) 
// Note that the individual channel commands need not be in any particular order (this
// is to maintain backward compatibility with the previous version of this protocol, which
// allowed only a single channel command per packet.

// The internal representation is as follows:
typedef struct
{
  char command;
  char controller_id[2];
  char all_off;
  char data[MAX_CHANNELS];
} tpacket;


void clear_errors(void);


void print_errors(void);

int have_error(void);

#endif // FLG_PACKET_DEF


