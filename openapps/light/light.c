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

#define DEBUG               TRUE
#define THRESHOLD_TEST      TRUE

//=========================== variables =======================================

light_vars_t        light_vars;

//=========================== prototypes ======================================

void light_send_task_cb(void);

//=========================== public ==========================================

void light_init() 
{  
   // clear local variables
   memset(&light_vars,0,sizeof(light_vars_t));
  
#if THRESHOLD_TEST == TRUE
   
   if (light_checkMyId(SENSOR_ID) &&
       sensors_is_present(SENSOR_LIGHT))
   {
      callbackRead_cbt             sixtop_light_read_cb;
      uint16_t                     lux = 0;
      
      sixtop_light_read_cb = sensors_getCallbackRead(SENSOR_LIGHT);
      lux = sixtop_light_read_cb();
      
      openserial_printInfo(COMPONENT_LIGHT,ERR_LIGHT_THRESHOLD,
                          (errorparameter_t)lux,
                          0);
   }
#endif
}

void light_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

bool light_is_initialized(void)
{
  return light_vars.initialized;
}

void light_initialize(bool state)
{
  light_vars.initialized = state;
}

bool light_state(void)
{
  return light_vars.state;
}

uint16_t light_counter(void)
{
  return light_vars.counter;
}

void light_send(uint16_t lux, bool state)
{
  
  light_vars.lux = lux;
  light_vars.state = state;
  
  scheduler_push_task(light_send_task_cb, TASKPRIO_MAX);
}

void light_receive(OpenQueueEntry_t* pkt) 
{
   OpenQueueEntry_t* fw;
   
   // don't run if not synched
   if (ieee154e_isSynch() == FALSE)
   {
     openqueue_freePacketBuffer(pkt);
     return;
   }
   
   // ownserhip
   pkt->owner = COMPONENT_LIGHT;
   
   // retrieve the counter
   int16_t counter = pkt->payload[0] | (pkt->payload[1] << 8);

   // retrieve the state
   uint16_t state = pkt->payload[2];
   
   // free the packet
   openqueue_freePacketBuffer(pkt);

#if DEBUG == TRUE
        openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_RCV,
                 (errorparameter_t)counter,
                 (errorparameter_t)state);
#endif
   
   // drop if we already received this packet
   if (counter <= light_vars.counter)
   {
      openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_DROP,
                   (errorparameter_t)counter,
                   (errorparameter_t)state);
      return;
   }
   
   // update the counter 
   light_vars.counter = counter;
     
   // if I am the sink process the message (update the state)
   if (light_checkMyId(SINK_ID)) 
   {  
     /* TURN ON/OFF THE LIGHT ON THE SINK */
 
     if (state != light_vars.state)
     {
        openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_STATE,
                          (errorparameter_t)state,
                          (errorparameter_t)0);
        light_vars.state = state;
     }
     
     return;
   }
   
   // I am not the sink, lets forward
   fw = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
   if (fw==NULL) {
     openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,0,0);
     return;
   }
   
   fw->owner                         = COMPONENT_LIGHT;
   fw->creator                       = COMPONENT_LIGHT;

   // payload
   packetfunctions_reserveHeaderSize(fw,3);
   *((uint16_t*)&fw->payload[0]) = counter;
   *((uint8_t*)&fw->payload[2]) = state;
       
   fw->l2_nextORpreviousHop.type        = ADDR_16B;
   fw->l2_nextORpreviousHop.addr_16b[0] = 0xff;
   fw->l2_nextORpreviousHop.addr_16b[1] = 0xff;

#if DEBUG == TRUE
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_FW,
                       (errorparameter_t)counter,
                       (errorparameter_t)state);
#endif
       
   if ((sixtop_send(fw))==E_FAIL) {
      openqueue_freePacketBuffer(fw);
   }
}

//=========================== private =========================================

void light_send_task_cb() 
{
   OpenQueueEntry_t*    pkt;
   uint8_t i;
   
   for (i = 0; i < 3; i++)
   {
      light_vars.counter++;
      
      // get a free packet buffer
      pkt = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
      if (pkt==NULL) {
          openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,0,0);
          return;
       }
       
       pkt->owner                         = COMPONENT_LIGHT;
       pkt->creator                       = COMPONENT_LIGHT;

       // payload
       packetfunctions_reserveHeaderSize(pkt,3);
       *((uint16_t*)&pkt->payload[0]) = light_vars.counter;
       *((uint8_t*)&pkt->payload[2]) = light_vars.state;
       
       pkt->l2_nextORpreviousHop.type        = ADDR_16B;
       pkt->l2_nextORpreviousHop.addr_16b[0] = 0xff;
       pkt->l2_nextORpreviousHop.addr_16b[1] = 0xff;

#if DEBUG == TRUE    
       openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_SEND,
                         (errorparameter_t)light_vars.counter,
//                         (errorparameter_t)light_vars.lux,
                         (errorparameter_t)light_vars.state);
#endif
       
       if ((sixtop_send(pkt))==E_FAIL) {
          openqueue_freePacketBuffer(pkt);
       }
   }
}

bool light_checkMyId(uint16_t addr)
{
  return ((idmanager_getMyID(ADDR_64B)->addr_64b[7] == (addr & 0xff)) &&
          (idmanager_getMyID(ADDR_64B)->addr_64b[6] == (addr >> 8)));
}