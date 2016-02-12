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
#include "leds.h"

//=========================== variables =======================================

light_vars_t        light_vars;

//=========================== prototypes =======================================

void light_send_one_packet(uint8_t pktId);

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
   leds_light_off();
}

//=== transmitting

/**
\brief Trigger the light app, which can decide to send a packet.
*/
void light_trigger(void) {
   bool                 iShouldSend;
   uint8_t              pktId;
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
      leds_light_on();
      iShouldSend = TRUE;
   } else if (light_vars.light_state==TRUE  && (light_vars.light_reading <  (LUX_THRESHOLD - LUX_HYSTERESIS))) {
      // light was just turned off
      
      light_vars.light_state = FALSE;
      debugpins_light_clr();
      leds_light_off();
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
   
   // increment the burstId
   light_vars.burstId = (light_vars.burstId+1)%16;
   
   // send burst of LIGHT_BURSTSIZE packets
   for (pktId=0;pktId<LIGHT_BURSTSIZE;pktId++) {
      light_send_one_packet(pktId);
   }
}

uint8_t  light_get_light_info(uint8_t pktId) {
   return (light_vars.burstId<<4) | (pktId<<1) | light_vars.light_state;
}

port_INLINE void light_send_one_packet(uint8_t pktId) {
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
   ((light_ht*)(pkt->payload))->type        = LONGTYPE_DATA;
   ((light_ht*)(pkt->payload))->src         = idmanager_getMyShortID();
   ((light_ht*)(pkt->payload))->light_info  = light_get_light_info(pktId);
   
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
   eb_ht*          rxPkt;
   uint8_t         pkt_burstId;
   uint8_t         pkt_light_state;
   
   // handle the packet
   do {
      
      // take ownserhip over the packet
      pkt->owner        = COMPONENT_LIGHT;
      
      // parse packet
      rxPkt             = (eb_ht*)pkt->payload;
      pkt_burstId       = (rxPkt->light_info & 0xf0)>>4;
      pkt_light_state   = (rxPkt->light_info & 0x01)>>0;
      
      // filter burstID
      if (pkt_burstId!=light_vars.burstId) {
         if ( ((pkt_burstId-light_vars.burstId)&0x0f) <=7) {
            // new burstID
            
            // reset pktIDMap
            light_vars.pktIDMap = 0x00;
            
            // remove old packets from queue
            // TODO Fix #17
            
         } else {
            // old burstID
            
            // abort
            break;
         }
      }
      
      // update the burstId, pktId and light_state
      light_vars.burstId     = pkt_burstId;
      light_vars.light_state = pkt_light_state;
      
      // map received light_state to light debug pin
      if (light_vars.light_state==TRUE) {
         debugpins_light_set();
         leds_light_on();
      } else {
         debugpins_light_clr();
         leds_light_off();
      }
      
   } while(0);
   
   // free packet
   openqueue_freePacketBuffer(pkt);
}

void light_receive_data(OpenQueueEntry_t* pkt) {
   light_ht*         rxPkt;
   uint8_t           pkt_burstId;
   uint8_t           pkt_pktId;
   uint8_t           pkt_light_state;
   
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
      rxPkt             = (light_ht*)pkt->payload;
      pkt_burstId       = (rxPkt->light_info & 0xf0)>>4;
      pkt_pktId         = (rxPkt->light_info & 0x07)>>1;
      pkt_light_state   = (rxPkt->light_info & 0x01)>>0;
      
      // filter burstID
      if (pkt_burstId!=light_vars.burstId) {
         if ( ((pkt_burstId-light_vars.burstId)&0x0f) <=7) {
            // new burstID
            
            // reset pktIDMap
            light_vars.pktIDMap = 0x00;
            
            // remove old packets from queue
            // TODO Fix #17
            
         } else {
            // old burstID
            
            // abort
            break;
         }
      }
      
      // abort if this is a pktId I already sent
      if ( light_vars.pktIDMap & (1<<pkt_pktId) ) {
          // already sent
          break;
      } else {
          // remember I sent that one
          light_vars.pktIDMap |= (1<<pkt_pktId);
      }
      
      // update the burstId, pktId and light_state
      light_vars.burstId     = pkt_burstId;
      light_vars.light_state = pkt_light_state;
      
      // map received light_state to light debug pin
      if (light_vars.light_state==TRUE) {
         debugpins_light_set();
         leds_light_on();
      } else {
         debugpins_light_clr();
         leds_light_off();
      }
      
      // retransmit packet
      if (idmanager_getMyShortID()!=SINK_ID) {
         light_send_one_packet(pkt_pktId);
      }
   } while(0);
   
   // free the packet
   openqueue_freePacketBuffer(pkt);
}

//=========================== private ==========================================
