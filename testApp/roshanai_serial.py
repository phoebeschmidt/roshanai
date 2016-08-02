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
import time
from rtmidi import *
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


class RealTimeMidiThread(Thread):
    def __init__(self, ser, input_id, programs):
        Thread.__init__(self)
        self.serial = ser
        self.input_id = input_id
        self.programs = programs
        self.current_program = programs[0]

    def run(self):
        timer = time.time()

        print_device_info()
        midi_in = MidiIn(0)

        if self.input_id is None and len(midi_in.get_ports) > 0:
            self.input_id = 0

        print ("using input_id :%s:" % self.input_id)
        midi_in.open_port(self.input_id)
        port_name = midi_in.get_port_name(self.input_id)
        global running
        while True:
            if (not running):
                print "Thread finished running"
                break
            while True:
                msg = midi_in.get_message()

                if msg:
                    message, deltatime = msg
                    timer += deltatime
                    status = message[0] & 0xf0 # bit mask for the status message so we ignore channel number
                    if status == 0x90: # Note on
                        note = message[1]
                        velocity = message[2]
                        if velocity == 0:
                            self.sendSignal(note, 0) # note on with velocity 0 is a note off
                        else:
                            self.sendSignal(note, 1) # note on
                    if status == 0x80: # Note off
                        note = message[1]
                        self.sendSignal(note, 0)
                    if status == 0xc0: # Program change
                        program = message[1]
                        self.change_program(program)
                    print("[%s] @%0.6f %r" % (port_name, timer, message))

    def change_program(self, program_index):
        self.current_program = self.programs[program_index]

    def sendSignal(self, midi_note, onOff):
        '''
        controller_id - int
        midi_note -
        onOff - 0 or 1
        '''
        try:
            board_id, relay_id = self.current_program[midi_note]
        except:
            board_id = (int(midi_note) / 8) + 1 # python defaults to floor division for ints. Every 8 notes up we move to another board
            relay_id = (int(midi_note) % 8) + 1

        sendString = "!%02d" % board_id
        dataString = "%i%s" % (relay_id, onOff)
        sendString = sendString + dataString + "." # "." means end of command. "~" means you can send multiple
        print sendString
        self.serial.write(sendString.encode())

    def stopSignals(self):
        ''' Stop sending signals, send 'all off' command to terminate pattern'''
        sendString = "!0000."
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
