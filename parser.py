#!/usr/bin/python
import Tkinter
import serial, threading
import _winreg as winreg
import struct
import os
import time
import topology

class fieldParsingKey(object):

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
        self.inputData = open(inputFileName,'rb')
        self.inputData = self.inputData.read()
        print len(self.inputData)
        if len(self.inputData) > 0:
            # print "-------------------"
            self.outputFile = open(inputFileName[:-4]+'_parsed.txt','w')
        self.inputBuf = ""
        self.lastByte = '\x7e'
        self.numOfSerialFrame = 0
        self.fieldStructure = []
        self.addFieldStruct()
        threading.Thread.__init__(self)
        
    def run(self):
        fileIndex = 0
        for rxByte in self.inputData:
            if rxByte != '\x7e' and self.lastByte == '\x7e':
                # start of frame
                self.inputBuf           += '\x7e'
                self.inputBuf           += rxByte
            elif rxByte != '\x7e':
                # middle of frame
                self.inputBuf           += rxByte
            elif rxByte == '\x7e':
                # end of frame
                self.numOfSerialFrame += 1
                # print self.numOfSerialFrame
                self.inputBuf           += rxByte
                self.inputBuf     = self.inputBuf[1:-1]
                self.inputBuf = self.inputBuf.replace(self.HDLC_ESCAPE+self.HDLC_FLAG_ESCAPED,   self.HDLC_FLAG)
                self.inputBuf = self.inputBuf.replace(self.HDLC_ESCAPE+self.HDLC_ESCAPE_ESCAPED, self.HDLC_ESCAPE)
                self.parsePayload(self.inputBuf)
                self.inputBuf = ""
                    
            self.lastByte = rxByte
        if len(self.inputData)>0:
            self.outputFile.close()
    
    def parsePayload(self,payload):
        # print payload
        if len(payload) < 4:
            self.bufIndex = 0
            return 
        payloadType = payload[0] 
        if payloadType == 'S':
            moteID = struct.unpack('<BB',payload[1:3])
            statusType = ord(payload[3])
            for entry in self.fieldStructure:
                # print entry.val, statusType
                if entry.val == statusType:
                    content = struct.unpack(entry.structure,payload[4:4+entry.length])
                    for i in range(len(entry.fields)):
                        self.outputFile.write(entry.fields[i]+'='+str(content[i])+'\n')
                    self.outputFile.write('\n')
                    return
            print "Wrong status type {0}, row content={1}".format(statusType,payload[4:])
            
            
        elif payloadType == 'I' or payloadType == 'E' or payloadType == 'C':
            # print [chr(ord(c)) for c in self.inputBuf]
            moteID = struct.unpack('<BB',payload[1:3])
            content = struct.unpack('<BBHH',payload[3:9])
            self.outputFile.write('calling_component='+str(content[0])+'\n')
            self.outputFile.write('error_code='+str(content[1])+'\n')
            self.outputFile.write('argument 1='+str(content[2])+'\n')
            self.outputFile.write('argument 2='+str(content[3])+'\n')
            self.outputFile.write('\n')
        else:
            print "Wrong payload type {0}, row payload={1}".format(payloadType,payload[1:])
        
    def addFieldStruct(self):
        self.fieldStructure.append(fieldParsingKey(
                1,
                0,
                'IsSync',
                '<B',
                [
                    'isSync',                    # B
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(
                21,
                1,
                'IdManager',
                '<BBBBBBBBBBBBBBBBBBBBB',
                [
                    'isDAGroot',                 # B
                    'myPANID_0',                 # B
                    'myPANID_1',                 # B
                    'my16bID_0',                 # B
                    'my16bID_1',                 # B
                    'my64bID_0',                 # B
                    'my64bID_1',                 # B
                    'my64bID_2',                 # B
                    'my64bID_3',                 # B
                    'my64bID_4',                 # B
                    'my64bID_5',                 # B
                    'my64bID_6',                 # B
                    'my64bID_7',                 # B
                    'myPrefix_0',                # B
                    'myPrefix_1',                # B
                    'myPrefix_2',                # B
                    'myPrefix_3',                # B
                    'myPrefix_4',                # B
                    'myPrefix_5',                # B
                    'myPrefix_6',                # B
                    'myPrefix_7',                # B
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(   
                2,
                2,
                'MyDagRank',
                '<H',
                [
                    'myDAGrank',                 # H
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(
                4,
                3,
                'OutputBuffer',
                '<HH',
                [
                    'index_write',               # H
                    'index_read',                # H
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(
                5,
                4,
                'Asn',
                '<BHH',
                [
                    'asn_4',                     # B
                    'asn_2_3',                   # H
                    'asn_0_1',                   # H
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(
                15,
                5,
                'MacStats',
                '<BBhhBII',
                [
                    'numSyncPkt' ,               # B
                    'numSyncAck',                # B
                    'minCorrection',             # h
                    'maxCorrection',             # h
                    'numDeSync',                 # B
                    'numTicsOn',                 # I
                    'numTicsTotal',              # I
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(
                12,
                6,
                'ScheduleRow',
                '<BHBBBBBHH',
                [
                    'row',                       # B
                    'slotOffset',                # H 
                    'channelOffset',             # B
                    'type',                      # B
                    'numRx',                     # B
                    'numTx',                     # B
                    'lastUsedAsn_4',             # B
                    'lastUsedAsn_2_3',           # H
                    'lastUsedAsn_0_1',           # H
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(
                20,
                7,
                'QueueRow',
                '<BBBBBBBBBBBBBBBBBBBB',
                [
                    'creator_0',                 # B
                    'owner_0',                   # B
                    'creator_1',                 # B
                    'owner_1',                   # B
                    'creator_2',                 # B
                    'owner_2',                   # B
                    'creator_3',                 # B
                    'owner_3',                   # B
                    'creator_4',                 # B
                    'owner_4',                   # B
                    'creator_5',                 # B
                    'owner_5',                   # B
                    'creator_6',                 # B
                    'owner_6',                   # B
                    'creator_7',                 # B
                    'owner_7',                   # B
                    'creator_8',                 # B
                    'owner_8',                   # B
                    'creator_9',                 # B
                    'owner_9',                   # B
                ],
            )
        )
        self.fieldStructure.append(fieldParsingKey(
                18,
                8,
                'NeighborsRow',
                '<BBBBBHHbBBBBHH',
                [
                    'row',                       # B
                    'used',                      # B
                    'parentPreference',          # B
                    'stableNeighbor',            # B
                    'switchStabilityCounter',    # B
                    'shortID',                   # H
                    'DAGrank',                   # H
                    'rssi',                      # b
                    'numRx',                     # B
                    'numTx',                     # B
                    'numWraps',                  # B
                    'asn_4',                     # B
                    'asn_2_3',                   # H
                    'asn_0_1',                   # H
                ],
            )
        )

        
def addInNeighborTable(newNeighbor, neighborTable):
    for neighbor in neighborTable:
        if neighbor == newNeighbor:
            return
    neighborTable += [newNeighbor]
# ========================= main ==========================
if __name__ == "__main__":
    for fileName in os.listdir('./'):
        if len(fileName) == 7:
            print fileName
            temp = parser(fileName)
            temp.start()
            # raw_input()
            
    time.sleep(1)
    
    # define moteId variable
    motes = []
    moteList = []
    outputFile = open('neighborInf.txt','w')
    for fileName in os.listdir('./'):
        if len(fileName)>10 and fileName[-10:-4] == 'parsed':
            moteId = ''
            neighborTable = []
            parsedFile = open(fileName,'r')
            line = parsedFile.readline()
            findMyId = False
            findMyDagrank = False
            print '======== New File (Mote) {0} Begin ========\n'.format(fileName[:3])
            while line != '':
                if line.find('my16bID_0')==0 and findMyId == False:
                    print '---------------'
                    print 'my16bID_0={0:2x}   |'.format(int(line[line.find('=')+1:-1]))
                    moteId = int(line[line.find('=')+1:-1])
                    line = parsedFile.readline()
                    print 'my16bID_1={0:2x}   |'.format(int(line[line.find('=')+1:-1]))
                    moteId = int(line[line.find('=')+1:-1])*256+moteId
                    print '---------------\n'
                    addInNeighborTable(moteId,moteList)
                    findMyId = True
                if line.find('myDAGrank')==0 and findMyDagrank == False:
                    print '---------------'
                    print 'myDAGrank={0:3}  |'.format(line[line.find('=')+1:-1])
                    print '---------------\n'
                    findMyDagrank = True
                if line.find('row')==0:
                    row = line[line.find('=')+1:-1]
                    line = parsedFile.readline()
                    if line.find('used')==0:
                        line = parsedFile.readline()
                        if line.find('parentPreference')==0:
                            output = ''
                            output +='row={0} '.format(row)
                            output+='parentPreference={0} '.format(line[line.find('=')+1:-1])
                            line = parsedFile.readline()
                            output+='stableNeighbor={0} '.format(line[line.find('=')+1:-1])
                            line = parsedFile.readline()
                            output+='switchStabilityCounter={0} '.format(line[line.find('=')+1:-1])
                            line = parsedFile.readline()
                            if int(line[line.find('=')+1:]) != 0:
                                output+='shortID={0:2x} {1:2x} (H L)'.format(int(line[line.find('=')+1:])/256,int(line[line.find('=')+1:])%256)
                                addInNeighborTable(int(line[line.find('=')+1:]),neighborTable)
                                addInNeighborTable(int(line[line.find('=')+1:]),moteList)
                                line = parsedFile.readline()
                                output+='DAGrank={0} '.format(line[line.find('=')+1:-1])
                                line = parsedFile.readline()
                                output+='rssi={0} '.format(line[line.find('=')+1:-1])
                            else:
                                output = ''
                            if output != '':
                                print output
                                outputFile.write(output+'\n')
                line = parsedFile.readline()
                
            motes += [[moteId,neighborTable]]
    outputFile.close()
    # plot topology
    # topologyFrame = topology.topology(moteList)
    # topologyFrame.myCanvas.delete("all")
    # topologyFrame._generateAxises()
    # topologyFrame._drawMotes(moteList)
    # moteIndex = 0
    # while moteIndex<len(motes):
        # topologyFrame.deleteLines()
        # topologyFrame._drawLines(motes[moteIndex])
        # topologyFrame.myCanvas.update()
        # moteIndex += 1
        # # time.sleep(1)
    # Tkinter.mainloop()
        