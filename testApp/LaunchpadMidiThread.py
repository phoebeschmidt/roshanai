from roshanai_serial import RealTimeMidiThread

class LaunchpadMidiThread(RealTimeMidiThread):
    def __init__(self, ser, input_id):
        self.noteToSolenoidMap = {}
        self.relayAndBoardToNoteMap = {}
        super(LaunchpadMidiThread, self).__init__(ser, input_id)

    def noteToBoardId(self, note):
        mapped_board = self.noteToSolenoidMap[note][0]
        return mapped_board

    def noteToRelayId(self, note):
        mapped_relay = self.noteToSolenoidMap[note][1]
        return int(note) % 8

    def createMapping(self, note, board_id, relay_id):
        self.noteToSolenoidMap[note] = (board_id, relay_id)
        self.relayAndBoardToNoteMap[(board_id, relay_id)] = note

    def relayAndBoardToNote(self, board_id, relay_id):
        return self.relayAndBoardToNoteMap[(board_id, relay_id)]
