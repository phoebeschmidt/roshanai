import BaseHTTPServer
import os
# relativePath = 'whackerTester'
class WhackerTesterHttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_GET(self):
        # if self.path.endswith(".png"):
        #         f=open("." + self.path, "rb")
        #         self.send_response(200)
        #         self.send_header('Content-type','image/png')
        #         self.end_headers()
        #         self.wfile.write(f.read())
        #         f.close()
        #         return
        # elif self.path.endswith(".gif"):
        #         f=open("." + self.path, "rb")
        #         self.send_response(200)
        #         self.send_header('Content-type','image/gif')
        #         self.end_headers()
        #         self.wfile.write(f.read())
        #         f.close()
        #         return
        # elif self.path.endswith(".jpeg"):
        #         f=open("." + self.path, "rb")
        #         self.send_response(200)
        #         self.send_header('Content-type','image/jpeg')
        #         self.end_headers()
        #         self.wfile.write(f.read())
        #         f.close()
        #         return
        if self.path.endswith(".html"):
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
        elif self.path.endswith(".md"):
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
            return

    # def do_POST(self):
    #     global requestQueue
    #
    #     ctype, pdict = parse_header(self.headers['content-type'])
    #     if ctype == 'multipart/form-data':
    #         postvars = parse_multipart(self.rfile, pdict)
    #     elif ctype == 'application/x-www-form-urlencoded':
    #         length = int(self.headers['content-length'])
    #         postvars = parse_qs(
    #                 self.rfile.read(length),
    #                 keep_blank_values=1)
    #     else:
    #         postvars = {}
    #     if self.path == "/sendsignals":
    #         pattern = postvars["pattern"][0]
    #         print pattern
    #         if not pattern:
    #             return
    #         jsonpattern = json.loads(pattern)
    #         print jsonpattern
    #         signalQueue.put(jsonpattern) # would like to make this self.requestQueue XXX
    #         self.send_response(200)
    #     elif self.path == "/stopsignals":
    #         signalQueue.put({"stopSignals", True})
    #         self.send_response(200)
    #     else:
    #         self.send_response(404)
    #
