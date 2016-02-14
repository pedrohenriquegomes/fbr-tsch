#!/usr/bin/python
import Tkinter
import random as rm
import math
import time

# =========================== define ====================================

NUMOFMOTES           = 10  # number of motes
LEVELOFNUMOFLINKS    = 10  # level of number of links: 10 ~ 0 (no connection). If LINKDISTANCE < MINIMUMDISTANCE: Full Connections network.
MINIMUMDISTANCE      = 40  # minimum distance between two motes
LINKDISTANCE         = 100 # possible distance for a connection (works only when larger than MINIMUMDISTANCE)
DIAMETEROFMOTE       = 12  # size of mote

NUMOFCHANNEL = 16
NUMOFSLOT    = 23

BLACKLISTLENGTH = 20

# ============================ typedef ==================================

class cellInfo():
    def __init__(self):
        self.used = False
        self.tsNum = 0
        self.choffset = 0

# ============================ help functions ===========================
def calculateDistance(x1,y1,x2,y2):
    return math.sqrt((abs(x1-x2))**2+(abs(y1-y2))**2)

# ============================ class =====================================
class topology():

    def __init__(self,motesList):
        # initialize parent class
        self.master = Tkinter.Tk()
        self.numOfMotes = len(motesList)
        self.motesList = motesList
        
        # create axis list
        self.xaxis = [0 for i in range(self.numOfMotes)]
        self.yaxis = [0 for i in range(self.numOfMotes)]
        
        self.neighbourTable = [[False for i in range(self.numOfMotes)] for i in range(self.numOfMotes)]
        self.blacklistTxTable = [cellInfo() for i in range(BLACKLISTLENGTH)]
        self.blacklistRxTable = [cellInfo() for i in range(BLACKLISTLENGTH)]
        
        self.myCanvas = Tkinter.Canvas(self.master, width=600, height=400)
        self.myCanvas.pack()
        
        self.slotLabel    = Tkinter.Label(self.master,text="slotoffset") 
        self.channelLabel = Tkinter.Label(self.master,text="choffset")
        # self.blacklistLink= Tkinter.Label(self.master,text="blacklist for link mote3 -> mote1")
        
    def _generateAxises(self,startX = 20, endX = 580, startY = 20, endY = 380 ):
        # generate motes axises
        for i in range(self.numOfMotes):
            while True:
                self.xaxis[i] = rm.randrange(startX,endX)
                self.yaxis[i] = rm.randrange(startY,endY)
                state = True
                for j in range(i):
                    if abs(self.xaxis[i]-self.xaxis[j]) < MINIMUMDISTANCE and abs(self.yaxis[i]-self.yaxis[j]) < MINIMUMDISTANCE:
                        state = False
                        break
                if state == False:
                    continue
                else:
                    break
                    
    def _drawOneLine(self,mote1x,mote1y,mote2x,mote2y):
        self.myCanvas.create_line(mote1x,mote1y,mote2x,mote2y)
        
    def _drawLinesByNeighbourTable(self):
        for i in range(self.numOfMotes):
            for j in range(self.numOfMotes):
                if self.neighbourTable[i][j] == True:
                    self._drawOneLine(self.xaxis[i],self.yaxis[i],self.xaxis[j],self.yaxis[j])
                   
    def _drawLines(self,moteTopology=None):
        # create lines for motes 
        if moteTopology == None:
            for i in range(self.numOfMotes):
                for j in range(self.numOfMotes):
                    if ( (not (i is j)) and LEVELOFNUMOFLINKS >= rm.randrange(1,10) ): 
                        if ( calculateDistance(self.xaxis[i],self.yaxis[i],self.xaxis[j],self.yaxis[j]) < LINKDISTANCE ):
                            self.myCanvas.create_line(self.xaxis[i],self.yaxis[i],self.xaxis[j],self.yaxis[j])
        else:
            id = moteTopology[0]
            idx,idy = self.findIndex(id)
            if idx != None:
                for neighbor in moteTopology[1]:
                    neighborx, neighbory = self.findIndex(neighbor)
                    if neighborx != None:
                        self.myCanvas.create_line(idx,idy,neighborx,neighbory)
    def _drawMotes(self,moteList=None):
        # create motes
        for i in range(self.numOfMotes):
            self.myCanvas.create_oval(self.xaxis[i]-DIAMETEROFMOTE, self.yaxis[i]-DIAMETEROFMOTE, \
                          self.xaxis[i]+DIAMETEROFMOTE, self.yaxis[i]+DIAMETEROFMOTE, \
                          fill="blue")
            if moteList == None:
                self.myCanvas.create_text(self.xaxis[i],self.yaxis[i],text=str(i+1))
            else:
                self.myCanvas.create_text(self.xaxis[i],self.yaxis[i],text='{0:x}|{1:x}'.format(int(moteList[i])/256,int(moteList[i])%256))
            
    def findIndex(self,id):
        # print id
        for i in range(len(self.motesList)):
            # print self.motesList[i]
            if id == self.motesList[i]:
                return self.xaxis[i], self.yaxis[i]
        return None,None
        
    def _setNeighbourTable(self,mote1,mote2,state):
        self.neighbourTable[mote1][mote2] = state
        self.neighbourTable[mote2][mote1] = state
        
    def _markBlacklist(self,index,used,tsNum,choffset,type):
        if type == 0:   #Tx blacklist
            blacklist = self.blacklistTxTable
        else: #Rx blacklist
            blacklist = self.blacklistRxTable
        
        blacklist[index].used = used
        blacklist[index].tsNum = tsNum
        blacklist[index].choffset = choffset

# ============================== main ============================
if __name__ == "__main__":
    topologyFrame = topology()
    while True:
        topologyFrame.myCanvas.delete("all")
        # topologyFrame._drawCheck()
        # topologyFrame._fillCellsInBlacklist()
        topologyFrame._generateAxises()
        topologyFrame._drawLines()
        topologyFrame._drawMotes()
        topologyFrame.myCanvas.update()
        time.sleep(2)
    Tkinter.mainloop()