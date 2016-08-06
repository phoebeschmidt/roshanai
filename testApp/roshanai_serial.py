# python serial driver for FLG test code
#
import serial
import os
import Queue
import sys
import BaseHTTPServer
from realTimeMidiThread import *
from whackerTesterHttpHandler import WhackerTesterHttpHandler
from signalSendingThread import SignalSendingThread

signalQueue = None

class FakeSerial:
    def write(self, input):
        print input


FAKE_SERIAL=False
def initSerial():
    if FAKE_SERIAL:
        return FakeSerial()
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
    if "--fake_serial" in sys.argv or "-f" in sys.argv:
        FAKE_SERIAL = True

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
        finally:
            real_time_midi_thread.stop()
    elif "--pattern" in sys.argv or "-p" in sys.argv:
        signalQueue = Queue.Queue()
        signalProcessingThread = SignalSendingThread(ser, signalQueue)
        signalProcessingThread.start()
        httpd = BaseHTTPServer.HTTPServer(("localhost", PORT), WhackerTesterHttpHandler, signalQueue)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print "Keyboard interrupt detected, terminating"
        finally:
            signalProcessingThread.stop()
            httpd.server_close()
    else:
        usage()
