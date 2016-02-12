#include "opendefs.h"
#include "topology.h"
#include "idmanager.h"

//=========================== defines =========================================

#define FORCETOPOLOGY

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

/**
\brief Force a topology.

This function is used to force a certain topology, by hard-coding the list of
acceptable neighbors for a given mote. This function is invoked each time a
packet is received. If it returns FALSE, the packet is silently dropped, as if
it were never received. If it returns TRUE, the packet is accepted.

Typically, filtering packets is done by analyzing the IEEE802.15.4 header. An
example body for this function which forces a topology is:

   switch (idmanager_getMyID(ADDR_64B)->addr_64b[7]) {
      case TOPOLOGY_MOTE1:
         if (ieee802514_header->src.addr_64b[7]==TOPOLOGY_MOTE2) {
            returnVal=TRUE;
         } else {
            returnVal=FALSE;
         }
         break;
      case TOPOLOGY_MOTE2:
         if (ieee802514_header->src.addr_64b[7]==TOPOLOGY_MOTE1 ||
             ieee802514_header->src.addr_64b[7]==TOPOLOGY_MOTE3) {
            returnVal=TRUE;
         } else {
            returnVal=FALSE;
         }
         break;
      default:
         returnVal=TRUE;
   }
   return returnVal;

By default, however, the function should return TRUE to *not* force any
topology.

\param[in] ieee802514_header The parsed IEEE802.15.4 MAC header.

\return TRUE if the packet can be received.
\return FALSE if the packet should be silently dropped.
*/
bool topology_isAcceptablePacket(uint16_t shortID) {
#ifdef FORCETOPOLOGY
#define MOTE_A 0x6f16
#define MOTE_B 0x3bdd
#define MOTE_C 0x7905
#define MOTE_D 0xb957
#define MOTE_E 0xbb5e
#define MOTE_F 0x930f
   bool returnVal;
   
   returnVal=FALSE;
   /*
   switch (idmanager_getMyShortID()) {
      case MOTE_A:
         if (
               shortID==MOTE_B ||
               shortID==MOTE_C
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_B:
         if (
               shortID==MOTE_A ||
               shortID==MOTE_D ||
               shortID==MOTE_E
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_C:
         if (
               shortID==MOTE_A ||
               shortID==MOTE_D ||
               shortID==MOTE_E
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_D:
         if (
               shortID==MOTE_B ||
               shortID==MOTE_C ||
               shortID==MOTE_F
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_E:
         if (
               shortID==MOTE_B ||
               shortID==MOTE_C ||
               shortID==MOTE_F
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_F:
         if (
               shortID==MOTE_D ||
               shortID==MOTE_E
            ) {
            returnVal=TRUE;
         }
         break;
   }
   */
   switch (idmanager_getMyShortID()) {
      case MOTE_A:
         if (
               shortID==MOTE_B
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_B:
         if (
               shortID==MOTE_A ||
               shortID==MOTE_C
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_C:
         if (
               shortID==MOTE_B ||
               shortID==MOTE_D
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_F:
         if (
               shortID==MOTE_C ||
               shortID==MOTE_E
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_E:
         if (
               shortID==MOTE_D ||
               shortID==MOTE_F
            ) {
            returnVal=TRUE;
         }
         break;
      case MOTE_D:
         if (
               shortID==MOTE_E
            ) {
            returnVal=TRUE;
         }
         break;
   }
   return returnVal;
#else
   return TRUE;
#endif
}

//=========================== private =========================================