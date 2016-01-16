#include "opendefs.h"
#include "processIE.h"
#include "sixtop.h"
#include "idmanager.h"
#include "openserial.h"
#include "IEEE802154.h"
#include "openqueue.h"
#include "neighbors.h"
#include "IEEE802154E.h"
#include "schedule.h"
#include "scheduler.h"
#include "packetfunctions.h"
#include "light.h"

//=========================== variables =======================================

//=========================== public ==========================================

//===== prepend IEs

port_INLINE uint8_t processIE_prependCounterIE(OpenQueueEntry_t* pkt){

   // inserting counter and state into the EB
   packetfunctions_reserveHeaderSize(pkt,3);
   
   *((uint16_t*)&pkt->payload[0]) = light_counter();
   *((uint8_t*)&pkt->payload[2]) = light_state();
       
   return 3;
}

port_INLINE uint8_t processIE_prependSyncIE(OpenQueueEntry_t* pkt){

   // reserve space
   packetfunctions_reserveHeaderSize(
      pkt,
      sizeof(sync_IE_ht)
   );
   
   // Keep a pointer to where the ASN will be
   // Note: the actual value of the current ASN and JP will be written by the
   //    IEEE802.15.4e when transmitting
   pkt->l2_ASNpayload               = pkt->payload; 
   
   return 6;
}
