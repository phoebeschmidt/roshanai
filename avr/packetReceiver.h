/*******************************
 *
 * Packet Receiver Interface
 * 
 * packetReceiver.h
 * 
 * Copyright 2016 FLG
 *
 *  -- CSW 
 *
 *******************************/

void packetReceiver_init(int address, volatile long int *timer);
int  packetReceiver_isPacketAvailable(void);
void packetReceiver_parsePacket(void);
