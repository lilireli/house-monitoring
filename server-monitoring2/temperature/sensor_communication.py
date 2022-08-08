import zmq
import threading

import logging

class SensorCommunication(threading.Thread):
    def __init__(self):
        self.context = None
        self.socket = None
        threading.Thread.__init__(self)

    def initialize_socket(self):
        logging.info("Starting ZMQ thread")
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)    
        self.socket.bind("tcp://*:5555")

    def run(self):
        self.initialize_socket()

        while 1:
            #  Wait for next request from client
            message = self.socket.recv()
            logging.warning("Received request: %s" % message)

            #  Send reply back to client
            self.socket.send(b"World")