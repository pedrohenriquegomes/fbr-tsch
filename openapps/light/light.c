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

// transmitting
void     light_format_packet(OpenQueueEntry_t* pkt);
// receiving
void     light_consume_packet(void);
// forward
void     light_fwd_packet(void);

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
   
   debugpins_rxlight_clr();
   debugpins_txlight_clr();
}

//=== transmitting

/**
\brief Trigger the light app, which can decide to send a packet.
*/
void light_trigger(void) {
   bool                 iShouldSend;
   uint8_t              i;
   OpenQueueEntry_t*    pktToSend;
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
      debugpins_txlight_set();
      iShouldSend = TRUE;
   } else if (light_vars.light_state==TRUE  && (light_vars.light_reading <  (LUX_THRESHOLD - LUX_HYSTERESIS))) {
      // light was just turned off
      
      light_vars.light_state = FALSE;
      debugpins_txlight_clr();
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
      
      // get a free packet buffer
      pktToSend = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
      if (pktToSend==NULL) {
         openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,0,0);
         return;
      }
      
      // format
      light_format_packet(pktToSend);
      
      // send
      if ((sixtop_send(pktToSend))==E_FAIL) {
         openqueue_freePacketBuffer(pktToSend);
      }
   }
   
}

port_INLINE void light_format_packet(OpenQueueEntry_t* pkt) {
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
   
   // don't run if not synched
   if (ieee154e_isSynch() == FALSE) {
      openqueue_freePacketBuffer(pkt);
      return;
   }
   
   // take ownserhip over the packet
   pkt->owner = COMPONENT_LIGHT;
   
   // parse packet
   rxPkt = (light_ht*)pkt->payload;
   
   // handle the packet
   do {
      // abort if not sync'ed
      if (ieee154e_isSynch()==FALSE) {
         break;
      }
      
      // abort if I'm the SENSOR node
      if (idmanager_getMyShortID()==SENSOR_ID) {
         break;
      }
      
      // abort if this an old packet already received this packet
      if (rxPkt->seqnum < light_vars.seqnum) {
         break;
      }
      
      // update the seqnum and light_state
      light_vars.seqnum      = rxPkt->seqnum;
      light_vars.light_state = rxPkt->light_state;
      
      // process packet
      if (idmanager_getMyShortID()==SINK_ID) {
         // I'm the sink: consume
         light_consume_packet();
      } else {
         // I'm NOT the sink: forward
         light_fwd_packet();
      }
   } while(0);
   
   // free the packet
   openqueue_freePacketBuffer(pkt);
}

void light_consume_packet(void) {
   
   // switch rxlight pin high/low
   if (light_vars.light_state==TRUE) {
      debugpins_rxlight_set();
   } else {
      debugpins_rxlight_clr();
   }
}

void light_fwd_packet(void) {
  // TODO Fix in #19
}

//=========================== private ==========================================
