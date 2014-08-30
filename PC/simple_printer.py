# -*- coding: utf-8 -*-
"""
Created on Fri Aug 15 20:20:18 2014

@author: Peter
"""

from __future__ import print_function
from PyQt4.QtGui import *
from PyQt4.QtCore import *
import sys
import serial
import locale
import struct
import Queue
import threading
import time
import __builtin__

encoding = locale.getdefaultlocale()[1]

quitFlag = threading.Event()
serialBuffer = Queue.Queue()

class GUI(QWidget):

    def __init__(self):
        super(GUI, self).__init__()

        self.initUI()

 
    def initUI(self):
        
        qbtn = QPushButton('Quit', self)
        qbtn.clicked.connect(QCoreApplication.instance().quit)
        qbtn.resize(140, 20)
        qbtn.move(30, 20)
        
        self.setGeometry(600, 300, 200, 200)
        self.setWindowTitle('Quit button')    
        self.show()

        tmr = QTimer(self)
        tmr.connect(updateLCD)
        tmr.start(100)
								
def readSerialData():
    
    print('Opening serial port and log file')
    #ser = serial.Serial('COM4', 230400, timeout=5)

    # As long as we don't want to quit, read as many characters
    # as are available in the buffer into a local queue. I'm using a
    # second layer of queue so that I can minimize the number of calls to
    # ser.read, which I assume is not very efficient.
    while True:
        # read as many bytes as available
        #data = ser.read(ser.inWaiting())

        # put the bytes into a local 'serialBuffer'
        for j in data:
            serialBuffer.put(j)

        # if we want to quit the program!
        if quitFlag.isSet():
            break
        
    print('\n closing serial port and log file')
    logfile.close()
    ser.close()
    
    return

def parseSerialData():
	return
	
def main():

    # Start a thread reading serial data into 'serialBuffer'
    rsd = threading.Thread(target=readSerialData)
    rsd.daemon = True
    rsd.start()

    # Start a thread to parse serial data
    psd = threading.Thread(target=parseSerialData)
    psd.daemon = True
    psd.start()

    # Open a small GUI window with a quit button
    app = QApplication(sys.argv)
    gui = GUI()
    app.exec_()

    # Once the GUI has exited (user pushed the quit button), notify
    # the threads to quit and wait for them to finish
    quitFlag.set()
    rsd.join()
    psd.join()

    sys.exit()

if __name__ == '__main__':
    main()