#include "opendefs.h"
#include "topology.h"
#include "idmanager.h"

//=========================== defines =========================================

#define MOTE_8  0x3bdd
#define MOTE_4  0xb957
#define MOTE_17 0xbb5e
#define MOTE_19 0x7905
#define MOTE_21 0x930f
#define MOTE_23 0x13cf
#define MOTE_25 0x6e29
#define MOTE_27 0x89a5
#define MOTE_28 0x5a53
#define MOTE_12 0x6f16

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

bool topology_isAcceptablePacket(uint16_t srcShortID) {
   bool returnVal;

#ifdef TOPOLOGY_LINEAR
   returnVal=FALSE;
   switch (idmanager_getMyShortID()) {
      case MOTE_8:
         if (srcShortID==MOTE_4) {                          returnVal=TRUE;}
         break;
      case MOTE_4:
         if (srcShortID==MOTE_8  || srcShortID==MOTE_17) {  returnVal=TRUE;}
         break;
      case MOTE_17:
         if (srcShortID==MOTE_4  || srcShortID==MOTE_19) {  returnVal=TRUE;}
         break;
      case MOTE_19:
         if (srcShortID==MOTE_17 || srcShortID==MOTE_21) {  returnVal=TRUE;}
         break;
      case MOTE_21:
         if (srcShortID==MOTE_19 || srcShortID==MOTE_23) {  returnVal=TRUE;}
         break;
      case MOTE_23:
         if (srcShortID==MOTE_21 || srcShortID==MOTE_25) {  returnVal=TRUE;}
         break;
      case MOTE_25:
         if (srcShortID==MOTE_23 || srcShortID==MOTE_27) {  returnVal=TRUE;}
         break;
      case MOTE_27:
         if (srcShortID==MOTE_25 || srcShortID==MOTE_28) {  returnVal=TRUE;}
         break;
      case MOTE_28:
         if (srcShortID==MOTE_27 || srcShortID==MOTE_12) {  returnVal=TRUE;}
         break;
      case MOTE_12:
         if (srcShortID==MOTE_28                       ) {  returnVal=TRUE;}
         break;
   }
#else
   returnVal=TRUE;
#endif
   return returnVal;
}

//=========================== private =========================================