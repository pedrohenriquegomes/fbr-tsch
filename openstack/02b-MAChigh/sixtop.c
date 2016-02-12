#include "opendefs.h"
#include "sixtop.h"
#include "openserial.h"
#include "openqueue.h"
#include "neighbors.h"
#include "IEEE802154E.h"
//#include "iphc.h"
//#include "otf.h"
#include "packetfunctions.h"
#include "openrandom.h"
#include "scheduler.h"
#include "opentimers.h"
#include "debugpins.h"
#include "leds.h"
#include "processIE.h"
#include "IEEE802154.h"
#include "idmanager.h"
#include "schedule.h"
#include "light.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

owerror_t     sixtop_send_internal(OpenQueueEntry_t* msg);

//=========================== public ==========================================

void sixtop_init() {
}

//======= from upper layer

owerror_t sixtop_send(OpenQueueEntry_t *msg) {
   
   // set metadata
   msg->owner           = COMPONENT_SIXTOP;
   msg->l2_frameType    = IEEE154_TYPE_DATA;
   msg->l2_rankPresent  = FALSE;
   
   return sixtop_send_internal(msg);
}

//======= from lower layer

/**
\brief Send an EB.

This is one of the MAC management tasks. This function inlines in the
timers_res_fired() function, but is declared as a separate function for better
readability of the code.
*/
port_INLINE void sixtop_sendEB(void) {
   OpenQueueEntry_t* eb;
   
   if (neighbors_getMyDAGrank()==DEFAULTDAGRANK){
      // I have not acquired a DAGrank yet
      
      // delete EBs packets from openqueue
      openqueue_removeAllCreatedBy(COMPONENT_SIXTOP);
      
      // stop here
      return;
   }
   
   // get a free packet buffer
   eb = openqueue_getFreePacketBuffer(COMPONENT_SIXTOP);
   if (eb==NULL) {
      openserial_printError(COMPONENT_SIXTOP,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      return;
   }
   
   // declare ownership over that packet
   eb->creator                              = COMPONENT_SIXTOP;
   eb->owner                                = COMPONENT_SIXTOP;
   
   // fill in EB
   packetfunctions_reserveHeaderSize(eb,sizeof(eb_ht));
   ((eb_ht*)(eb->payload))->type            = LONGTYPE_BEACON;
   ((eb_ht*)(eb->payload))->src             = idmanager_getMyShortID();
   ((eb_ht*)(eb->payload))->rank            = neighbors_getMyDAGrank();
   ((eb_ht*)(eb->payload))->light_info      = light_get_light_info(0);
   
   // remember where to write the ASN to
   eb->l2_ASNpayload                        = (uint8_t*)(&((eb_ht*)(eb->payload))->asn0);
   
   // some l2 information about this packet
   eb->l2_frameType                         = IEEE154_TYPE_BEACON;
   eb->l2_nextORpreviousHop.type            = ADDR_16B;
   eb->l2_nextORpreviousHop.addr_16b[0]     = 0xff;
   eb->l2_nextORpreviousHop.addr_16b[1]     = 0xff;
   
   // put in queue for MAC to handle
   sixtop_send_internal(eb);
}

void task_sixtopNotifSendDone(void) {
   OpenQueueEntry_t* msg;
   
   // get recently-sent packet from openqueue
   msg = openqueue_sixtopGetSentPacket();
   if (msg==NULL) {
      openserial_printCritical(
         COMPONENT_SIXTOP,
         ERR_NO_SENT_PACKET,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   // take ownership
   msg->owner = COMPONENT_SIXTOP;
   
   // send the packet to where it belongs
   switch (msg->creator) {
      
      case COMPONENT_SIXTOP:
         // this is a EB
            
         // discard packets
         openqueue_freePacketBuffer(msg);
         
         break;
  
      case COMPONENT_LIGHT:
         // this is a data packet
         
         light_sendDone(msg,msg->l2_sendDoneError);
         break;
         
      default:
         // send the rest up the stack
         break;
   }
}

void task_sixtopNotifReceive(void) {
   OpenQueueEntry_t*    msg;
   eb_ht*               eb;
   
   // get received packet from openqueue
   msg = openqueue_sixtopGetReceivedPacket();
   if (msg==NULL) {
      openserial_printCritical(
         COMPONENT_SIXTOP,
         ERR_NO_RECEIVED_PACKET,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   // take ownership
   msg->owner = COMPONENT_SIXTOP;
   
   // parse as if it's an EB (light_ht and eb_ht) start with the same bytes
   eb = (eb_ht*)msg->payload;
   
   // update neighbor statistics
   neighbors_indicateRx(
      eb->src,
      msg->l1_rssi,
      &msg->l2_asn
   );
   
   // send the packet up the stack, if it qualifies
   switch (*((uint16_t*)(msg->payload))) {
      case LONGTYPE_BEACON:
         neighbors_indicateRxEB(msg);
         light_receive_beacon(msg);
         break;
      case LONGTYPE_DATA:
        light_receive_data(msg);
        break;
      default:
         // free the packet's RAM memory
         openqueue_freePacketBuffer(msg);
         // log the error
         openserial_printError(
            COMPONENT_SIXTOP,
            ERR_MSG_UNKNOWN_TYPE,
            (errorparameter_t)msg->l2_frameType,
            (errorparameter_t)0
         );
         break;
   }
}

//======= debugging

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_myDAGrank(void) {
   uint16_t output;
   
   output = 0;
   
   output = neighbors_getMyDAGrank();
   openserial_printStatus(STATUS_DAGRANK,(uint8_t*)&output,sizeof(uint16_t));
   return TRUE;
}

//=========================== private =========================================

/**
\brief Transfer packet to MAC.

This function adds a IEEE802.15.4 header to the packet and leaves it the 
OpenQueue buffer. The very last thing it does is assigning this packet to the 
virtual component COMPONENT_SIXTOP_TO_IEEE802154E. Whenever it gets a change,
IEEE802154E will handle the packet.

\param[in] msg The packet to the transmitted
\param[in] iePresent Indicates wheter an Information Element is present in the
   packet.
\param[in] frameVersion The frame version to write in the packet.

\returns E_SUCCESS iff successful.
*/
owerror_t sixtop_send_internal(OpenQueueEntry_t* msg) {
   
   // assign a number of retries
   msg->l2_retriesLeft = 1;
   // this is a new packet which I never attempted to send
   msg->l2_numTxAttempts = 0;
   // transmit with the default TX power
   msg->l1_txPower = TX_POWER;
   // change owner to IEEE802154E fetches it from queue
   msg->owner  = COMPONENT_SIXTOP_TO_IEEE802154E;
   return E_SUCCESS;
}

