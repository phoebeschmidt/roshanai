import Queue
import json
from cgi import parse_header, parse_multipart
import time
from stoppableThread import StoppableThread
from sys import version as python_version
if python_version.startswith('3'):
    from urllib.parse import parse_qs
else:
    from urlparse import parse_qs


class SignalSendingThread(StoppableThread):
    def __init__(self, ser, requestQueue):
        ''' Constructor. '''
        StoppableThread.__init__(self)
        self.serial = ser
        self.requestQueue = requestQueue

    def run(self):
        ''' Run, until running flag is unset. Check status of flag every second '''
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
            if (not self.running):
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
