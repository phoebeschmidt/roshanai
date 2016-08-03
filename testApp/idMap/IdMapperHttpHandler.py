import BaseHTTPServer


class IdMapperHttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_GET(self):
        print self.path
        # else:
        #     self.send_response(404)
        #     self.send_header('Content-type','text/html')
        #     self.end_headers()
        #     self.wfile.write(get())
        #     return
    def do_POST(self):
        print 'inside do post'
        self.send_response(200)
        self.wfile.write("<html><body><h1>POST!</h1></body></html>")
        print self
        return
