avrdude -P /dev/cu.usbmodem621 -v -c stk500v1 -p m8 -b19200 -U lfuse:w:0xe2:m -U hfuse:w:0xd9:m


