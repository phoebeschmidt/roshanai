# Roshanai

This is forked from https://github.com/FlamingLotusGirls/pulse. We are going to add on to their serial framework in order to control the solenoids for striking various drums/shakers/blocks/etc.


What we've got here:

* avr - Code that runs on the 8-output relay driver boards (see eagle files in flg-svn)<br>
* testApp - Android and PC code for running the 8-output relay driver board. Can run on any android device, or web interface + python can run on Mac/Linux box. Requires standard FTDI USB to 485 cable to connect with board

Not in use for this project, but may have use later:
* audio - Sketches for ALSA audio drivers, incomplete<br>
* network - multicast listener for heartbeats, controls relay board to send heartbeat and other patterns. Python, runs on RPi and Mac (intended for RPi in the sculpture)<br>

To do, kind of in order of importance:

1. Read Midi files and play them.
1. Loop through library of midi files.
1. Record some kind of live extended performance.
1. Interface for managing midi files, play order, picking and launching them.
2. Live Midi input from a controller.
3. Basic web interface for mapping controller to solenoids. (I'll be using a Novation Launchpad for the beginning)
