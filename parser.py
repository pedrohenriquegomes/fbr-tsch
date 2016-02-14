#!/usr/bin/python
from Tkinter import *
import serial, threading
import _winreg as winreg
import struct
import os

class FieldParsingKey(object):

    def __init__(self,length,val,name,structure,fields):
        self.length      = length
        self.val        = val
        self.name       = name
        self.structure  = structure
        self.fields     = fields

# ================ threading =================
class parser(threading.Thread):
    HDLC_FLAG              = '\x7e'
    HDLC_FLAG_ESCAPED      = '\x5e'
    HDLC_ESCAPE            = '\x7d'
    HDLC_ESCAPE_ESCAPED    = '\x5d'
    
    def __init__(self,inputFileName):
        self.bufIndex = 0
        self.inputData = open(inputFileName,'r').read()
        if self.inputData > 0:
            self.outputFile = open(inputFileName[-4:]+'.txt','w')
        self.inputBuf = ""
        self.lastByte = '\x7e'
        self.fieldStructure = []
        threading.Thread.__init__(self)
        
    def run(self):
        fileIndex = 0
        while fileIndex < len(self.inputData):
            rxByte = self.inputData[fileIndex]
            fileIndex += 1
            if rxByte != '\x7e' and self.lastByte == '\x7e':
                # start of frame
                self.inputBuf           += '\x7e'
                self.inputBuf           += rxByte
            elif rxByte != '\x7e':
                # middle of frame
                self.inputBuf           += rxByte
            elif rxByte == '\x7e':
                # end of frame
                self.inputBuf           += rxByte
                self.inputBuf     = self.inputBuf[1:-1]
                self.inputBuf = self.inputBuf.replace(self.HDLC_ESCAPE+self.HDLC_FLAG_ESCAPED,   self.HDLC_FLAG)
                self.inputBuf = self.inputBuf.replace(self.HDLC_ESCAPE+self.HDLC_ESCAPE_ESCAPED, self.HDLC_ESCAPE)
                self.parsePayload(self.inputBuf)
                self.inputBuf = ""
                    
                self.lastByte = rxByte
        self.outputFile.close()
    
    def parsePayload(self,payload):
        if len(payload) < 4:
            self.bufIndex = 0
            return 
        payloadType = payload[0] 
        if payloadType == 'S':
            moteID = struct.unpack('<BB',payload[1:3])
            statusTyp = payload[3]
            for entry in self.fieldStructure:
                if entry.val[0] == statusTyp:
                    content = struct.unpack(entry.structure,payload[4:])
                    for i in range(len(entry.fields)):
                        self.outputFile.write(entry.fields[i]+'='+str(content[i]))
                    return
            print "Wrong status type {0}, row content={1}".format(statusTyp,payload[4:])
            
        elif payloadType == 'I' or payloadType == 'E' or payloadType == 'C':
            moteID = struct.unpack('<BB',payload[1:3])
            content = struct.unpack('<BBHH',payload[3:])
            self.ouputFile.write('calling_component='+str(content[0]))
            self.ouputFile.write('error_code='+str(content[1]))
            self.ouputFile.write('argument 1='+str(content[2]))
            self.ouputFile.write('argument 2='+str(content[3]))
            
        print "Wrong payload type {0}, row payload={1}".format(payload,payload[1:])
        
    def addFieldStruct(self):
        self.fieldStructure = []
        
# ========================= main ==========================
if __name__ == "__main__":
    for fileName in os.listdir():
        if len(fileName) == 7:
            temp = parser(fileName)
            temp.start()