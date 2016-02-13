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

bool topology_isAcceptablePacket(uint16_t shortID) {
   bool returnVal;
   
   returnVal=FALSE;
#ifdef TOPOLOGY_LINEAR
   switch (idmanager_getMyShortID()) {
      case MOTE_8:
         if (shortID==MOTE_4) {                       returnVal=TRUE;}
         break;
      case MOTE_4:
         if (shortID==MOTE_8  || shortID==MOTE_17) {  returnVal=TRUE;}
         break;
      case MOTE_17:
         if (shortID==MOTE_4  || shortID==MOTE_19) {  returnVal=TRUE;}
         break;
      case MOTE_19:
         if (shortID==MOTE_17 || shortID==MOTE_21) {  returnVal=TRUE;}
         break;
      case MOTE_21:
         if (shortID==MOTE_19 || shortID==MOTE_23) {  returnVal=TRUE;}
         break;
      case MOTE_23:
         if (shortID==MOTE_21 || shortID==MOTE_25) {  returnVal=TRUE;}
         break;
      case MOTE_25:
         if (shortID==MOTE_23 || shortID==MOTE_27) {  returnVal=TRUE;}
         break;
      case MOTE_27:
         if (shortID==MOTE_25 || shortID==MOTE_28) {  returnVal=TRUE;}
         break;
      case MOTE_28:
         if (shortID==MOTE_27 || shortID==MOTE_12) {  returnVal=TRUE;}
         break;
      case MOTE_12:
         if (shortID==MOTE_28                    ) {  returnVal=TRUE;}
         break;
   }
#else
   return TRUE;
#endif
}

//=========================== private =========================================