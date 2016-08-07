from stoppableThread import StoppableThread
import time
from rtmidi import *

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

class RealTimeMidiThread(StoppableThread):
    def __init__(self, ser, input_id, programs):
        StoppableThread.__init__(self)
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
        while True:
            if (not self.running):
                print "Thread finished running"
                break
            else:
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
