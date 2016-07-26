# python serial driver for FLG test code
#
import serial
from threading import Thread
import BaseHTTPServer
import time
import os
import Queue
import json
import sys
from cgi import parse_header, parse_multipart
from sys import version as python_version
if python_version.startswith('3'):
    from urllib.parse import parse_qs
else:
    from urlparse import parse_qs

import pygame
import pygame.midi
from pygame.locals import *


running = True
signalQueue = None

def print_device_info():
    pygame.midi.init()
    _print_device_info()
    pygame.midi.quit()

class RealTimeMidiThread(Thread):
    def __init__(self, ser, input_id):
        Thread.__init__(self)
        self.serial = ser
        self.input_id = input_id

    def run(self):
        pygame.init()
        pygame.fastevent.init()
        pygame.midi.init()

        _print_device_info()

        if self.input_id is None:
            input_id = pygame.midi.get_default_input_id()
        else:
            input_id = self.input_id

        print ("using input_id :%s:" % input_id)
        i = pygame.midi.Input(input_id)

        pygame.display.set_mode((1,1))

        global running
        while True:
            if (not running):
                break

            pygame.fastevent.pump() # this needs to be done in order for pygame to not freeze up
            if i.poll():
                midi_events = i.read(10)
                # convert them into pygame events.
                midi_evs = pygame.midi.midis2events(midi_events, i.device_id)

                for m_e in midi_evs:
                    print m_e
                    note = m_e.data1
                    velocity = m_e.data2
                    if m_e.data2 == 0:
                        onOff = 0
                    else:
                        onOff = 1
                    board_id = noteToBoardId(note)
                    relay_id = noteToRelayId(note)
                    print "sending signal: board:%d relay:%d onOff:%d" % (board_id, relay_id, onOff)
                    self.sendSignal(board_id, note % 8, onOff)
        del i
        pygame.midi.quit()
    def noteToBoardId(self, note):
        return int(note) / 8 # python defaults to floor division for ints. Every 8 notes up we move to another board
    def noteToRelayId(self, note):
        return int(note) % 8

    def sendSignal(self, board_id, relay_id, onOff):
        '''
        controller_id - int
        number - int 0-7 for the specific relay to fire
        onOff - 0 or 1
        '''
        sendString = "!%02d" % board_id
        dataString = "%i%s" % (relay_id, onOff)
        sendString = sendString + dataString + "." # "." means end of command. "~" means you can send multiple
        print sendString
        self.serial.write(sendString.encode())


class SignalSendingThread(Thread):
    def __init__(self, ser, requestQueue):
        ''' Constructor. '''
        Thread.__init__(self)
        self.serial = ser
        self.requestQueue = requestQueue

    def run(self):
        ''' Run, until running flag is unset. Check status of flag every second '''
        global running
        timeout = 1.0
        while True:
            # block until there's something to do
            try:
                signalRequest = self.requestQueue.get(True, timeout)
                if "stopSignals" in signalRequest :
                    self.stopSignals()
                else:
                    controller_id     = signalRequest["boardId"]
                    timeslice     = signalRequest["timeslice"]
                    repeat        = signalRequest["repeat"]
                    signalArray   = signalRequest["signals"]
                    print "timeslice is ", timeslice
                    print "timeslice int is %d" % int(timeslice)
                    print "timeslice float is %f" % float(timeslice)
                    timeslice_sec = float(timeslice)/1000
                    print "timeslice /1000 is %f" % timeslice_sec
                    timeslice
                    self.sendSignals(controller_id, float(timeslice)/1000, signalArray, repeat)
                    print "Repeat is ", repeat
            except Queue.Empty:
                pass
                # debug printf - timeout!
            if (not running):
                break


    def sendSignal(self, controller_id, signalArray):
        ''' Send timesliced signals to the serial port, one for each whacker '''
        sendString = "!0%s" % controller_id
        for i in range(0, len(signalArray)):
            onOff = signalArray[i]
            dataString = "%i%s" % (i+1, onOff)
            if (i == len(signalArray)-1) :
                sendString = sendString + dataString + "."
            else:
                sendString = sendString + dataString + "~"
        print sendString
        self.serial.write(sendString.encode())

    def sendSignals(self, controller_id, timeslice, signalArray, repeat):
        ''' Send multiple timeslices to the serial port. May repeat '''
        print "Timeslice is " , timeslice
        global running
        while True:
            for i in range(0, len(signalArray)):
                now = time.time()
                nextTime = now + timeslice
                self.sendSignal(controller_id, signalArray[i])
                time.sleep(nextTime - time.time())
            if not running:
                break;
            if not repeat:
                break
            elif not self.requestQueue.empty():
                break

    def stopSignals(self):
        ''' Stop sending signals, send 'all off' command to terminate pattern'''
        sendString = "!0000."
        self.serial.write(sendString.encode())






class WhackerTesterHttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.endswith(".png"):
                f=open("." + self.path, "rb")
                self.send_response(200)
                self.send_header('Content-type','image/png')
                self.end_headers()
                self.wfile.write(f.read())
                f.close()
                return
        elif self.path.endswith(".gif"):
                f=open("." + self.path, "rb")
                self.send_response(200)
                self.send_header('Content-type','image/gif')
                self.end_headers()
                self.wfile.write(f.read())
                f.close()
                return
        elif self.path.endswith(".jpeg"):
                f=open("." + self.path, "rb")
                self.send_response(200)
                self.send_header('Content-type','image/jpeg')
                self.end_headers()
                self.wfile.write(f.read())
                f.close()
                return
        elif self.path.endswith(".html"):
                f=open("." + self.path, "rb")
                self.send_response(200)
                self.send_header('Content-type','text/html')
                self.end_headers()
                self.wfile.write(f.read())
                f.close()
                return
        elif self.path.endswith(".js"):
                f=open("." + self.path, "rb")
                self.send_response(200)
                self.send_header('Content-type','application/javascript')
                self.end_headers()
                self.wfile.write(f.read())
                f.close()
                return
        else:
            self.send_response(404)
            self.send_header('Content-type','text/html')
            self.end_headers()
#            self.wfile.write(get())
            return

    def do_POST(self):
        ctype, pdict = parse_header(self.headers['content-type'])
        if ctype == 'multipart/form-data':
            postvars = parse_multipart(self.rfile, pdict)
        elif ctype == 'application/x-www-form-urlencoded':
            length = int(self.headers['content-length'])
            postvars = parse_qs(
                    self.rfile.read(length),
                    keep_blank_values=1)
        else:
            postvars = {}
        if self.path == "/sendsignals":
            pattern = postvars["pattern"][0]
            print pattern
            if not pattern:
                return
            jsonpattern = json.loads(pattern)
            print jsonpattern
            signalQueue.put(jsonpattern) # would like to make this self.requestQueue XXX
            self.send_response(200)
        elif self.path == "/stopsignals":
            signalQueue.put({"stopSignals", True})
            self.send_response(200)
        else:
            self.send_response(404)


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

def print_device_info():
    pygame.midi.init()
    _print_device_info()
    pygame.midi.quit()

def _print_device_info():
    for i in range( pygame.midi.get_count() ):
        r = pygame.midi.get_device_info(i)
        (interf, name, input_val, output_val, opened) = r

        in_out = ""
        if input_val:
            in_out = "(input)"
        if output_val:
            in_out = "(output)"

        print ("%2i: interface :%s:, name :%s:, opened :%s:  %s" %
               (i, interf, name, opened, in_out))

def usage():
    print ("--real_time_midi [device_id] : Real time input midi mode")
    print ("--pattern : Web based pattern player")
    print ("--list : list available midi devices")

if __name__ == '__main__':
    running = True
    PORT = 8666
    try:
        device_id = int( sys.argv[-1] )
    except:
        device_id = None
    ser = initSerial()
    if "--real_time_midi" in sys.argv or "-m" in sys.argv:

        real_time_midi_thread = RealTimeMidiThread(ser, device_id)
        real_time_midi_thread.start()
        try:
            while True:
                continue
        except KeyboardInterrupt:
            print "Keyboard interrupt detected, terminating"
            running = False
    elif "--list" in sys.argv or "-l" in sys.argv:
        print_device_info()
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
