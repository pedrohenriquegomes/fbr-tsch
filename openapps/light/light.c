#include "opendefs.h"
#include "sensors.h"
#include "light.h"
#include "neighbors.h"
#include "openqueue.h"
#include "openserial.h"
#include "opentimers.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "idmanager.h"
#include "IEEE802154E.h"
#include "sixtop.h"
#include "debugpins.h"
#include "openrandom.h"

//=========================== variables =======================================

light_vars_t        light_vars;

//=========================== prototypes =======================================

void light_send_one_packet(void);

//=========================== public ===========================================

//=== initialization

/**
\brief Initialize this module.
*/
void light_init(void) {
#ifdef LIGHT_PRINTOUT_READING
   callbackRead_cbt     light_read_cb;
   uint16_t             lux; 
#endif
   
   // clear local variables
   memset(&light_vars,0,sizeof(light_vars_t));
   
#ifdef LIGHT_PRINTOUT_READING
   // printout the current light reading, used to calibrate LUX_THRESHOLD
   if ( idmanager_getMyShortID()==SENSOR_ID && sensors_is_present(SENSOR_LIGHT) ) {
      
      light_read_cb     = sensors_getCallbackRead(SENSOR_LIGHT);
      lux               = light_read_cb();
      
      openserial_printInfo(
         COMPONENT_LIGHT,
         ERR_LIGHT_THRESHOLD,
         (errorparameter_t)lux,
         0
      );
   }
#endif
   
   debugpins_light_clr();
}

//=== transmitting

/**
\brief Trigger the light app, which can decide to send a packet.
*/
void light_trigger(void) {
   bool                 iShouldSend;
   uint8_t              i;
#ifdef LIGHT_FAKESEND
   uint16_t             numAsnSinceLastEvent;
#else
   callbackRead_cbt     light_read_cb;
#endif
   
   // stop if I'm not the SENSOR mote with a light sensor attached
   if ( idmanager_getMyShortID()!=SENSOR_ID || sensors_is_present(SENSOR_LIGHT)==FALSE ) {
      return;
   }
   
   //=== if I get here, I'm the SENSOR mote
   
#ifdef LIGHT_FAKESEND
   // how many cells since the last time I transmitted?
   numAsnSinceLastEvent = ieee154e_asnDiff(&light_vars.lastEventAsn);
   
   // set light_reading to fake high/low value to trigger packets
   if (numAsnSinceLastEvent>LIGHT_FAKESEND_PERIOD) {
      if (light_vars.light_reading<LUX_THRESHOLD) {
         light_vars.light_reading=2*LUX_THRESHOLD;
      } else {
         light_vars.light_reading=0;
      }
   }
#else
   // current light reading
   light_read_cb             = sensors_getCallbackRead(SENSOR_LIGHT);
   light_vars.light_reading  = light_read_cb();
#endif
   
   // detect light state switches
   if (       light_vars.light_state==FALSE && (light_vars.light_reading >= (LUX_THRESHOLD + LUX_HYSTERESIS))) {
      // light was just turned on
      
      light_vars.light_state = TRUE;
      debugpins_light_set();
      iShouldSend = TRUE;
   } else if (light_vars.light_state==TRUE  && (light_vars.light_reading <  (LUX_THRESHOLD - LUX_HYSTERESIS))) {
      // light was just turned off
      
      light_vars.light_state = FALSE;
      debugpins_light_clr();
      iShouldSend = TRUE;
   } else {
      // light stays in same state
      
      iShouldSend = FALSE;
   }
   
   // abort if no packet to send
   if (iShouldSend==FALSE) {
      return;
   }
   
   //=== if I get here, I send a packet
   
   // remember the current ASN
   ieee154e_getAsnStruct(&light_vars.lastEventAsn);
   
   // increment the seqnum
   light_vars.seqnum++;
   
   // send burst of LIGHT_BURSTSIZE packets
   for (i=0;i<LIGHT_BURSTSIZE;i++) {
      light_send_one_packet();
   }
}

port_INLINE void light_send_one_packet(void) {
   OpenQueueEntry_t*    pkt;
   
   // get a free packet buffer
   pkt = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
   if (pkt==NULL) {
      openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,0,0);
      return;
   }
   
   // take ownership over the packet
   pkt->owner                               = COMPONENT_LIGHT;
   pkt->creator                             = COMPONENT_LIGHT;
   
#ifdef LIGHT_CALCULATE_DELAY
   // TODO add light_vars.lastEventAsn into packet
#endif
   
   // fill payload
   packetfunctions_reserveHeaderSize(pkt,sizeof(light_ht));
   ((light_ht*)(pkt->payload))->type        = 0xdddd;
   ((light_ht*)(pkt->payload))->src         = idmanager_getMyShortID();
   ((light_ht*)(pkt->payload))->seqnum      = light_vars.seqnum;
   ((light_ht*)(pkt->payload))->light_state = light_vars.light_state;
   
   // send
   if ((sixtop_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}

void light_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   // cancel forwarding
   
   // free packet
   openqueue_freePacketBuffer(msg);
}

//== receiving

void light_receive_beacon(OpenQueueEntry_t* pkt) {
   // TODO: Fix #14
   
   // free packet
   openqueue_freePacketBuffer(pkt);
}

void light_receive_data(OpenQueueEntry_t* pkt) {
   light_ht*         rxPkt;
   
   // handle the packet
   do {
      // abort if I'm not sync'ed
      if (ieee154e_isSynch()==FALSE) {
         break;
      }
      
      // abort if I'm the SENSOR node
      if (idmanager_getMyShortID()==SENSOR_ID) {
         break;
      }
      
      // take ownserhip over the packet
      pkt->owner = COMPONENT_LIGHT;
      
      // parse packet
      rxPkt = (light_ht*)pkt->payload;
      
      // abort if this an old packet
      if (rxPkt->seqnum < light_vars.seqnum) {
         break;
      }
      
      // update the seqnum and light_state
      light_vars.seqnum      = rxPkt->seqnum;
      light_vars.light_state = rxPkt->light_state;
      
      // map received light_state to light debug pin
      if (light_vars.light_state==TRUE) {
         debugpins_light_set();
      } else {
         debugpins_light_clr();
      }
      
      // retransmit packet
      if (idmanager_getMyShortID()!=SINK_ID) {
        // TODO: Fix in #19
      }
   } while(0);
   
   // free the packet
   openqueue_freePacketBuffer(pkt);
}

//=========================== private ==========================================
