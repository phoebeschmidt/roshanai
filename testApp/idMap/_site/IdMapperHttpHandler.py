import BaseHTTPServer
import simplejson
from idMapper import IdMapper
from id_dict import id_dict

relativePath = 'idMap'

class IdMapperHttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def __init___(self):
        ''' Constructor '''
        BaseHTTPServer.BaseHTTPRequestHandler.__init__(self) #is this necessary?
        self.idMapper = IdMapper()

    def do_GET(self):
        print self.path
        if self.path.endswith(".html"):
            f=open(relativePath + self.path, "rb")
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(f.read())
            f.close()
            return
        # elif self.path=="/map/ids":
            # # TODO: figure out how to send id_dict values as json so we can initialize front end
            # self.send_response(200)
            # self.wfile.write("<h1>HELLO</h1>")
            # # print JSON.stringify(id_dict)
            # # return id_dict
            # simplejson.dumps(id_dict)
            # # self.wfile.close()
    def do_POST(self):
        content_len = int(self.headers.getheader('content-length', 0))
        data_string = self.rfile.read(content_len)
        body = simplejson.loads(data_string)
        if self.path=="/map/ids":
            if 'solenoidId' in body and 'relayAddress' in body:
                try:
                    idm = IdMapper()
                    # TODO: why does it only work when I instantiate idmapper here?
                    #TODO somehow have global id_dict
                    idm.setRelayAddress(body['solenoidId'], body['relayAddress'])
                    self.send_response(200)

                except:
                    self.send_response(500, "Error: something happened. Maybe solenoidid is incorrect")
            else:
                print "poorly formed request"
                self.send_response(422, "Malformed request. You must include a relayAddress and solenoidId.")
                return

        return
# else:
#     self.send_response(404)
#     self.send_header('Content-type','text/html')
#     self.end_headers()
#     self.wfile.write(get())
#     return
