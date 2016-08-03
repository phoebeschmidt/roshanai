# python serial driver for FLG test code
#
import serial
import os
import Queue
import sys
# from sys import version as python_version
# if python_version.startswith('3'):
#     from urllib.parse import parse_qs
# else:
#     from urlparse import parse_qs
# import time
from rtmidi import *
import realTimeMidiThread as RealTimeMidiThread
import whackerTesterHttpHandler as WhackerTesterHttpHandler
import signalSendingThread as SignalSendingThread

running = True
signalQueue = None

# These are defined in rtmidi, this provides multi platform support for different operating systems.
# Always use api 0 (default) or 1 (API_MACOSX_CORE:  "OS X CoreMIDI") on Mac
# ex. MidiIn(1), MidiIn(0)
apis = {
    API_MACOSX_CORE:  "OS X CoreMIDI",
    API_LINUX_ALSA:   "Linux ALSA",
    API_UNIX_JACK:    "Jack Client",
    API_WINDOWS_MM:   "Windows MultiMedia",
    API_RTMIDI_DUMMY: "RtMidi Dummy"
}

available_apis = get_compiled_api()


def initSerial():
    ser = serial.Serial()
    ser.baudrate = 19200
    port = False
    for filename in os.listdir("/dev"):
        if filename.startswith("tty.usbserial"):
            port = "/dev/" + filename
            print "Found usb serial at ", port
            break;

    if not port:
        sys.exit("No usb serial connected, aborting")

    ser.port = port
    ser.timeout =0
    ser.stopbits = serial.STOPBITS_ONE
    ser.bytesize = 8
    ser.parity   = serial.PARITY_NONE
    ser.rtscts   = 0  # XXX check what this means
    ser.open()
    return ser


def print_device_info(api=0):
    for api, api_name in sorted(apis.items()):
        if api in available_apis:
            for name, class_ in (("input", MidiIn), ("output", MidiOut)):
                try:
                    midi = class_(api)
                    ports = midi.get_ports()
                except StandardError as exc:
                    print("Could not probe MIDI %s ports: %s" % (name, exc))
                    continue
                if not ports:
                    print("No MIDI %s ports found." % name)
                else:
                    print("Available MIDI %s ports:\n" % name)
                    for port, name in enumerate(ports):
                        print("[%i] %s" % (port, name))
                print('')
                del midi

def usage():
    print ("--real_time_midi [device_id] : Real time input midi mode")
    print ("--pattern : Web based pattern player. Default of the pulse version of this script")
    print ("--list : list available midi devices")
    print ("--help : list the usage")


if __name__ == '__main__':
    if "--help" in sys.argv or "-h" in sys.argv or len(sys.argv) == 1:
        usage()
        sys.exit()
    elif "--list" in sys.argv or "-l" in sys.argv:
        print_device_info()

    running = True
    PORT = 8666
    try:
        device_id = int( sys.argv[-1] )
    except:
        device_id = None
    ser = initSerial()
    if "--real_time_midi" in sys.argv or "-m" in sys.argv:
        test_programs = [{0:(0,0)}] # this is only for testing now, we need to create real programs
        real_time_midi_thread = RealTimeMidiThread(ser, device_id, test_programs)
        real_time_midi_thread.start()
        try:
            while True:
                continue
        except KeyboardInterrupt:
            print "Keyboard interrupt detected, terminating"
            running = False
    elif "--pattern" in sys.argv or "-p" in sys.argv:
        signalQueue = Queue.Queue()
        signalProcessingThread = SignalSendingThread(ser, signalQueue)
        signalProcessingThread.start()
        httpd = BaseHTTPServer.HTTPServer(("localhost", PORT), WhackerTesterHttpHandler, signalQueue)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print "Keyboard interrupt detected, terminating"
            running = False
        httpd.server_close()
    else:
        usage()
