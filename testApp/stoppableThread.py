from threading import Thread

class StoppableThread(Thread):
    def __init__(self):
        Thread.__init__(self)
        self.running = True

    def stop(self):
        self.running = False
