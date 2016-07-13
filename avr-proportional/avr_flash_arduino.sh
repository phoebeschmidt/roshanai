avrdude -P /dev/cu.usbmodem621 -v -c stk500v1 -p m8 -b19200 -U flash:w:main.hex
#avrdude -P /dev/cu.usbmodem621 -v -c avrisp -p m8 -U flash:w:main.hex
