# Pulse Project

What we've got here:

BPM - Code to detect heartbeats and put the pulses out on the network
avr - Code that runs on the 8-output relay driver boards (see eagle files in flg-svn)
avr-proportional - code that runs on the proportional valve driver board (no eagle files ... yet)
test-proportional - Android code for running the proportional valve driver board. Can run on any android device, requires standard FTDI USB to 485 cable to connect with board
testApp - Android and PC code for running the 8-output relay driver board. Can run on any android device, or web interface + python can run on Mac/Linux box. Requires standard FTDI USB to 485 cable to connect with board
audio - Sketches for ALSA audio drivers, incomplete
network - multicast listener for heartbeats, controls relay board to send heartbeat and other patterns. Python, runs on RPi and Mac (intended for RPi in the sculpture)

Still to do:
LEDs. Lighting for pulse pods
Mobile Interactive. Code for whatever games on mobile interactive unit
Geek console. UI, controls for the geeks back at the geek table.
