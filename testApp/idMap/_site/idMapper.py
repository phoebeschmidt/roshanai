from id_dict import id_dict

class IdMapper:
    values = id_dict.values()
    def __init__(self):
        print "here i am"

    def setRelayAddress(self, solenoidId, relayAddr):
        print "before"
        print id_dict[int(solenoidId)]
        print int(solenoidId) > 0 and int(solenoidId) < 57
        if (int(solenoidId) > 0 and int(solenoidId) < 57):
            #add check for the address already being mapped tosomething
            id_dict[int(solenoidId)] = relayAddr
            # printMapping()
            print "after"
            print id_dict[int(solenoidId)]
            return
        else:
            return false

    def getRelayAddress(self, solenoidId):
        return id_dict[solenoidId]

    def printMapping(self):
        print "in printMapping"
        for x in id_dict:
            print x
            # print id_dict[x]
