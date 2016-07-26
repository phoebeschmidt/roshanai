from roshanai_serial import RealTimeMidiThread

class LaunchpadMidiThread(RealTimeMidiThread):
    def __init__(self, ser, input_id):
        self.noteMap = {}
        super(LaunchpadMidiThread, self).__init__(ser, input_id)

    def noteToBoardId(self, note):
        mapped_board = self.noteMap[note][0]
        return mapped_board

    def noteToRelayId(self, note):
        mapped_relay = self.noteMap[note][1]
        return int(note) % 8

    def createMapping(self, note, board_id, relay_id):
        self.noteMap[note] = (board_id, relay_id)
