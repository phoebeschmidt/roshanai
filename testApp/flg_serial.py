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


running = True
signalQueue = None

    
class SignalSendingThread(Thread):
    def __init__(self, ser , requestQueue):
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
        ''' Send timesliced signals to the serial port, one for each poofer '''
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
         
        
        
      


class PooferTesterHttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):
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
        global requestQueue
        
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


if __name__ == '__main__':
    running = True
    PORT = 8666

    ser = initSerial()
    signalQueue = Queue.Queue()
    signalProcessingThread = SignalSendingThread(ser, signalQueue)
    signalProcessingThread.start()
    httpd = BaseHTTPServer.HTTPServer(("localhost", PORT), PooferTesterHttpHandler, signalQueue)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print "Keyboard interrupt detected, terminating"
        running = False
    httpd.server_close()
    
