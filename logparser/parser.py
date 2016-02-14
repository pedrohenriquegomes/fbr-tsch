#!/usr/bin/python
import struct
import os
import time
from collections import namedtuple
import OpenHdlc
import traceback
import StackDefines
import pprint

class fieldParsingKey(object):
    def __init__(self,length,val,name,structure,fields):
        self.length      = length
        self.val        = val
        self.name       = name
        self.structure  = structure
        self.fields     = fields

class LogfileParser(object):
    
    HDLC_FLAG              = '\x7e'
    HDLC_FLAG_ESCAPED      = '\x5e'
    HDLC_ESCAPE            = '\x7d'
    HDLC_ESCAPE_ESCAPED    = '\x5d'
    
    def __init__(self):
        
        # parse
        alldata = self.parseAllFiles()
        
        # analyze
        allflatdata = []
        for (k,v) in alldata.items():
            allflatdata += v
        
        # question 1: are all preferred parents stable neighbors?
        output = []
        for d in allflatdata:
            if 'parentPreference' in d:
                if d['parentPreference']==2:
                    output += [ 'rssi={0} stableNeighbor={1}'.format(d['rssi'],d['stableNeighbor'])]
        with open('question_1.txt','w') as f:
            f.write('\n'.join(output))
        
        # question 2: how many errors?
        errorcount = {}
        for d in allflatdata:
            if 'errcode' in d:
                errstring = StackDefines.errorDescriptions[d['errcode']]
                if errstring not in errorcount:
                    errorcount[errstring] = 0
                errorcount[errstring] += 1
        with open('question_2.txt','w') as f:
            f.write(str(errorcount))
            
        # question 3: last neighbor table of each mote
        neighbortable = {}
        for (moteid,data) in alldata.items():
            neighbortable[moteid] = {}
            for d in data:
                if 'parentPreference' in d:
                    if d['used']==1:
                        neighbortable[moteid][d['row']] = d
        with open('question_3.txt','w') as f:
            pp = pprint.PrettyPrinter(indent=4)
            f.write(pp.pformat(neighbortable))
        
        # question 4: rssi histogram
        rssivals = {}
        for (moteid,data) in neighbortable.items():
            rssivals[moteid] = []
            for (_,v) in data.items():
                
                rssivals[moteid] += [(hex(v['shortID']),v['rssi'])]
        with open('question_4.txt','w') as f:
            output = []
            for (k,v) in rssivals.items():
                output += ['{0}: {1}'.format(k,sorted(v))]
            f.write('\n'.join(output))
        
        # question 5: network churn
        
    def parseAllFiles(self):
        alldata = {}
        for filename in os.listdir('./'):
            if filename.endswith('.txt') and (filename.startswith('1') or filename.startswith('2')):
                print 'Parsing {0}...'.format(filename),
                alldata[filename] = self.parseOneFile(filename)
                print 'done.'
        return alldata
    
    def parseOneFile(self,filename):
        self.hdlc  = OpenHdlc.OpenHdlc()
        (hdlcFrames,_) = self.hdlc.dehdlcify(filename)
        
        parsedFrames = []
        for f in hdlcFrames:
            # first byte is the type of frame
            if   f[0]==ord('D'):
                pf = self.parse_DATA(f[1:])
            elif f[0]==ord('S'):
                pf = self.parse_STATUS(f[1:])
            elif f[0]==ord('I'):
                pf = self.parse_INFO(f[1:])
            elif f[0]==ord('E'):
                pf = self.parse_ERROR(f[1:])
            elif f[0]==ord('C'):
                pf = self.parse_CRITICAL(f[1:])
            elif f[0]==ord('R'):
                pf = self.parse_REQUEST(f[1:])
            else:
                print 'TODO: parse frame of type {0}'.format(chr(f[0])) 
            if pf:
                parsedFrames += [pf]
        
        with open(filename+'.parsed','w') as f:
            f.write('\n'.join([str(pf) for pf in parsedFrames]))
        
        return parsedFrames
    
    #======================== level 1 parsers =================================
    
    def parse_DATA(self,frame):
        return {}
    def parse_STATUS(self,frame):
        header     = self.parseHeader(frame[:3],'<HB',('src','type'))
        payload    = {}
        if   header['type']==0: # IsSync
            payload = self.parseHeader(frame[3:],'<B',('isSync',))
        elif header['type']==1: # IdManager
            payload = self.parseHeader(
                frame[3:3+5],
                '<BHH',
                (
                    'isDAGroot',
                    'myPANID',
                    'my16bID',
                ),
            )
        elif header['type']==2: # MyDagRank
            payload = self.parseHeader(frame[3:],'<H',('myDAGrank',))
        elif header['type']==6: # ScheduleRow
            payload = self.parseHeader(
                frame[3:3+7],
                '<BHBBBB',
                (
                    'row',
                    'slotOffset',
                    'channelOffset',
                    'type',
                    'numRx',
                    'numTx',
                ),
            )
        elif header['type']==8: # NeighborsRow
            payload = self.parseHeader(
                frame[3:],
                '<BBBBBHHbBBBBHH',
                (
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
                ),
            )
        else:
            pass
        return payload
    def parse_INFO(self,frame):
        pass
    def parse_ERROR(self,frame):
        payload    = self.parseHeader(frame[:8],'<HBBHH',('moteID','component','errcode','arg1','arg2'))
        return payload
    def parse_CRITICAL(self,frame):
        pass
    def parse_REQUEST(self,frame):
        pass
    
    #======================== level 2 parsers =================================
    
    #======================== helpers =========================================
    
    def parseHeader(self,bytes,formatString,fieldNames):
        returnVal = {}
        fieldVals = struct.unpack(formatString, ''.join([chr(b) for b in bytes]))
        for (n,v) in zip(fieldNames,fieldVals):
            returnVal[n] = v
        return returnVal

#============================ main ============================================

def main():
    try:
        LogfileParser()
        raw_input("Script ended normally. Press Enter to close.")
    except Exception as err:
        print traceback.print_exc()
        raw_input("Script CRASHED. Press Enter to close.")

if __name__ == "__main__":
    main()
