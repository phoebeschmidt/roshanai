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
        else:
            self.send_response(404)
            self.send_header('Content-type','text/html')
            self.end_headers()
            # self.wfile.write(get())
            return
