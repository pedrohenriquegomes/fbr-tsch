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
   
   uint16_t counter = light_get_seqnum();
   
   *((uint8_t*)&pkt->payload[0]) = counter & 0xff;
   *((uint8_t*)&pkt->payload[1]) = counter >> 8;
   *((uint8_t*)&pkt->payload[2]) = light_get_light_state();
   
   return 3;
}
