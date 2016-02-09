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

sixtop_vars_t sixtop_vars;

//=========================== prototypes ======================================

// send internal
owerror_t     sixtop_send_internal(
   OpenQueueEntry_t*    msg,
   bool                 payloadIEPresent
);

// timer interrupt callbacks
void          sixtop_maintenance_timer_cb(opentimer_id_t id);

//=== EB task

void          timer_sixtop_management_fired(void);
void          sixtop_sendEB(void);

//=========================== public ==========================================

void sixtop_init() {
   
   //sixtop_vars.periodMaintenance  = 930 +(openrandom_get16b()&0x3f);
   sixtop_vars.periodMaintenance  = 92 +(openrandom_get16b()&0xf);
   sixtop_vars.busySendingEB      = FALSE;
   sixtop_vars.dsn                = 0;
   sixtop_vars.mgtTaskCounter     = 0;
   sixtop_vars.ebPeriod           = EBPERIOD;
   
   sixtop_vars.maintenanceTimerId = opentimers_start(
      sixtop_vars.periodMaintenance,
      TIMER_PERIODIC,
      TIME_MS,
      sixtop_maintenance_timer_cb
   );
}

void sixtop_setEBPeriod(uint8_t ebPeriod) {
   sixtop_vars.ebPeriod = ebPeriod;
}

uint8_t      sixtop_getEBPeriod(void) {
  return sixtop_vars.ebPeriod;
}

void sixtop_setHandler(six2six_handler_t handler) {
    sixtop_vars.handler = handler;
}

void sixtop_multiplyEBPeriod(uint8_t factor)
{
  sixtop_vars.ebPeriod *= factor;
}

void sixtop_addEBPeriod(uint8_t factor)
{
  sixtop_vars.ebPeriod += factor;
}

//======= from upper layer

owerror_t sixtop_send(OpenQueueEntry_t *msg) {
   
   // set metadata
   msg->owner        = COMPONENT_SIXTOP;
   msg->l2_frameType = IEEE154_TYPE_DATA;

   msg->l2_rankPresent = FALSE;

   if (msg->l2_payloadIEpresent == FALSE) {
      return sixtop_send_internal(
         msg,
         FALSE
      );
   } else {
      return sixtop_send_internal(
         msg,
         TRUE
      );
   }
}

//======= from lower layer

void task_sixtopNotifSendDone() {
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
   
   // update neighbor statistics
   if (msg->l2_sendDoneError==E_SUCCESS) {
      neighbors_indicateTx(
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         TRUE,
         &msg->l2_asn
      );
   } else {
      neighbors_indicateTx(
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         FALSE,
         &msg->l2_asn
      );
   }
   
   // send the packet to where it belongs
   switch (msg->creator) {
      
      case COMPONENT_SIXTOP:
         if (msg->l2_frameType==IEEE154_TYPE_BEACON) {
            // this is a EB
            
            // not busy sending EB anymore
            sixtop_vars.busySendingEB = FALSE;
         }
         // discard packets
         openqueue_freePacketBuffer(msg);
         
         // restart a random timer
         //sixtop_vars.periodMaintenance  = 930 +(openrandom_get16b()&0x3f);
         sixtop_vars.periodMaintenance  = 92 +(openrandom_get16b()&0xf);
         opentimers_setPeriod(
            sixtop_vars.maintenanceTimerId,
            TIME_MS,
            sixtop_vars.periodMaintenance
         );
         break;
  
      case COMPONENT_LIGHT:
         light_sendDone(msg,msg->l2_sendDoneError);
         break;
         
      default:
         // send the rest up the stack
         break;
   }
}

void task_sixtopNotifReceive() {
   OpenQueueEntry_t* msg;
   uint16_t          lenIE;
   
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
   
   // process the header IEs
   lenIE=0;
      
   // toss the header IEs
   packetfunctions_tossHeader(msg,lenIE);
   
   // update neighbor statistics
   neighbors_indicateRx(
      &(msg->l2_nextORpreviousHop),
      msg->l1_rssi,
      &msg->l2_asn,
      msg->l2_joinPriorityPresent,
      msg->l2_joinPriority
   );
   
   // reset it to avoid race conditions with this var.
   msg->l2_joinPriorityPresent = FALSE; 
   
   // send the packet up the stack, if it qualifies
   switch (msg->l2_frameType) {
      case IEEE154_TYPE_BEACON:
        // update the rank
         neighbors_indicateRxEB(msg);
         // if the beacon comes from an upper node we should check if the node is up-to-date
         light_receive_beacon(msg);
         openqueue_freePacketBuffer(msg);
         break;
      case IEEE154_TYPE_DATA:
        // we only have one type of data packet. It is from sixtop light application
        if (msg->length>0) {
           light_receive_data(msg);
         } else {
            // free up the RAM
            openqueue_freePacketBuffer(msg);
         }
        break;
      case IEEE154_TYPE_CMD:
      case IEEE154_TYPE_ACK:
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
bool debugPrint_myDAGrank() {
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
owerror_t sixtop_send_internal(
   OpenQueueEntry_t* msg, 
   bool    payloadIEPresent) {
   
   // assign a number of retries
   if (
      packetfunctions_isBroadcastMulticast(&(msg->l2_nextORpreviousHop))==TRUE
      ) {
      msg->l2_retriesLeft = 1;
   } else {
      msg->l2_retriesLeft = TXRETRIES + 1;
   }
   // record this packet's dsn (for matching the ACK)
   msg->l2_dsn = sixtop_vars.dsn++;
   // this is a new packet which I never attempted to send
   msg->l2_numTxAttempts = 0;
   // transmit with the default TX power
   msg->l1_txPower = TX_POWER;
   // add a IEEE802.15.4 header
   ieee802154_prependHeader(msg,
                            msg->l2_frameType,
                            payloadIEPresent,
                            msg->l2_dsn,
                            &(msg->l2_nextORpreviousHop)
                            );
   // change owner to IEEE802154E fetches it from queue
   msg->owner  = COMPONENT_SIXTOP_TO_IEEE802154E;
   return E_SUCCESS;
}

// timer interrupt callbacks

void sixtop_maintenance_timer_cb(opentimer_id_t id) {
  if (++sixtop_vars.mgtTaskCounter == sixtop_vars.ebPeriod)
  {
    scheduler_push_task(timer_sixtop_management_fired,TASKPRIO_SIXTOP);
    sixtop_vars.mgtTaskCounter = 0;
  }   
}

//======= EB/KA task

/**
\brief Timer handlers which triggers MAC management task.

This function is called in task context by the scheduler after the RES timer
has fired. This timer is set to fire every second, on average.

The body of this function executes one of the MAC management task.
*/
void timer_sixtop_management_fired(void) {
    sixtop_sendEB();
}

/**
\brief Send an EB.

This is one of the MAC management tasks. This function inlines in the
timers_res_fired() function, but is declared as a separate function for better
readability of the code.
*/
port_INLINE void sixtop_sendEB() {
   OpenQueueEntry_t* eb;
   uint8_t len;
   
   len = 0;
   
   if ((ieee154e_isSynch()==FALSE) || (neighbors_getMyDAGrank()==DEFAULTDAGRANK)){
      // I'm not sync'ed or I did not acquire a DAGrank
      
      // delete packets genereted by this module (EB and KA) from openqueue
      openqueue_removeAllCreatedBy(COMPONENT_SIXTOP);
      
      // I'm now busy sending an EB
      sixtop_vars.busySendingEB = FALSE;
      
      // stop here
      return;
   }
   
   if (sixtop_vars.busySendingEB==TRUE) {
      // don't continue if I'm still sending a previous EB
      return;
   }
   
   // if I get here, I will send an EB
   
   // get a free packet buffer
   eb = openqueue_getFreePacketBuffer(COMPONENT_SIXTOP);
   if (eb==NULL) {
      openserial_printError(COMPONENT_SIXTOP,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      return;
   }
   
   // declare ownership over that packet
   eb->creator = COMPONENT_SIXTOP;
   eb->owner   = COMPONENT_SIXTOP;
   
   // reserve space for EB-specific header
   // reserving for IEs.
   len += processIE_prependCounterIE(eb);
   len += processIE_prependSyncIE(eb);
  
   // some l2 information about this packet
   eb->l2_frameType                     = IEEE154_TYPE_BEACON;
   eb->l2_nextORpreviousHop.type        = ADDR_16B;
   eb->l2_nextORpreviousHop.addr_16b[0] = 0xff;
   eb->l2_nextORpreviousHop.addr_16b[1] = 0xff;
   
   // It has an IE in my payload
   eb->l2_payloadIEpresent   = TRUE;

   // lets embedd the rank
   eb->l2_rankPresent        = TRUE;
   eb->l2_rank               = neighbors_getMyDAGrank();
     
   // put in queue for MAC to handle
   sixtop_send_internal(eb,eb->l2_payloadIEpresent);
   
   // I'm now busy sending an EB
   sixtop_vars.busySendingEB = TRUE;
}
