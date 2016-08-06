from id_dict import id_dict

class IdMapper:
    values = id_dict.values()
    def __init__(self):
        print "here i am"

    def setRelayAddress(self, solonoidId, relayAddr):
        print "before"
        print id_dict[int(solonoidId)]
        print int(solonoidId) > 0 and int(solonoidId) < 57
        if (int(solonoidId) > 0 and int(solonoidId) < 57):
            #add check for the address already being mapped tosomething
            id_dict[int(solonoidId)] = relayAddr
            # printMapping()
            print "after"
            print id_dict[int(solonoidId)]
            return
        else:
            return false

    def getRelayAddress(self, solonoidId):
        return id_dict[solonoidId]

    def printMapping(self):
        print "in printMapping"
        for x in id_dict:
            print x
            # print id_dict[x]
