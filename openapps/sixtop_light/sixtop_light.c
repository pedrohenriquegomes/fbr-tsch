#include "opendefs.h"
#include "sensors.h"
#include "sixtop_light.h"
#include "neighbors.h"
#include "openqueue.h"
#include "openserial.h"
#include "opentimers.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "idmanager.h"
#include "IEEE802154E.h"
#include "sixtop.h"

#define DEBUG   TRUE

//=========================== variables =======================================

sixtop_light_vars_t        sixtop_light_vars;
callbackRead_cbt           sixtop_light_read_cb;

//=========================== prototypes ======================================

void sixtop_light_timer_cb(opentimer_id_t id);
void sixtop_light_task_cb(void);

//=========================== public ==========================================

void sixtop_light_init() {
   
   // clear local variables
   memset(&sixtop_light_vars,0,sizeof(sixtop_light_vars_t));
   
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] == SENSOR_ADDR) 
   {
      sixtop_light_vars.counter = 0;
   }
   else
   {
      sixtop_light_vars.counter = -1;
   }
   
   // start periodic timer
   sixtop_light_vars.timerId                    = opentimers_start(
      SIXTOP_LIGHT_PERIOD_MS,
      TIMER_PERIODIC,TIME_MS,
      sixtop_light_timer_cb
   );
}

void sixtop_light_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

void sixtop_light_receive(OpenQueueEntry_t* pkt) {
   OpenQueueEntry_t* fw;
   
   // don't run if not synch or does not know the RANK yet
   if (ieee154e_isSynch() == FALSE || neighbors_getMyDAGrank() == DEFAULTDAGRANK)
   {
     openqueue_freePacketBuffer(pkt);
     return;
   }
   
   //ownserhip
   pkt->owner = COMPONENT_LIGHT;
   
   // retrieve the counter
   int16_t counter = pkt->payload[0] | (pkt->payload[1] << 8);

   // retrieve the rank
   uint16_t rank = pkt->payload[2] | (pkt->payload[3] << 8);
   
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] == ROOT_ADDR) {
     if (counter > sixtop_light_vars.counter)
     {
       sixtop_light_vars.counter = counter;
     }
     openqueue_freePacketBuffer(pkt);

#if DEBUG == TRUE
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_RCV,
                 (errorparameter_t)counter,
                 (errorparameter_t)rank);
#endif
   
     return;
   }

#if DEBUG == TRUE
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_RCV,
                 (errorparameter_t)counter,
                 (errorparameter_t)rank);
#endif
   
   //check if rank is greater than ours
   if (rank > neighbors_getMyDAGrank())
   {  
        // get a new openqueuEntry_t for the retransmission
        fw = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
        if (fw==NULL) {
          openserial_printError(
             COMPONENT_LIGHT,
             ERR_NO_FREE_PACKET_BUFFER,
             (errorparameter_t)0,
             (errorparameter_t)0
          );
          return;
        }
   
       fw->owner                         = COMPONENT_LIGHT;
       fw->creator                       = COMPONENT_LIGHT;

       // payload
       packetfunctions_reserveHeaderSize(fw,sizeof(uint32_t));
       *((uint16_t*)&fw->payload[0]) = counter;
       *((uint16_t*)&fw->payload[2]) = neighbors_getMyDAGrank();
       
       fw->l2_nextORpreviousHop.type        = ADDR_16B;
       fw->l2_nextORpreviousHop.addr_16b[0] = 0xff;
       fw->l2_nextORpreviousHop.addr_16b[1] = 0xff;

#if DEBUG == TRUE
       openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_FW,
                         (errorparameter_t)counter,
                         (errorparameter_t)neighbors_getMyDAGrank());
#endif
       
       if ((sixtop_send(fw))==E_FAIL) {
          openqueue_freePacketBuffer(fw);
       }
   }
         
   openqueue_freePacketBuffer(pkt);
}

//=========================== private =========================================

void sixtop_light_timer_cb(opentimer_id_t id){
   
   scheduler_push_task(sixtop_light_task_cb, TASKPRIO_SIXTOP);
}

void sixtop_light_task_cb() {
   OpenQueueEntry_t*    pkt;
   uint8_t i;
   
   // don't run if not synch or does not know the RANK yet
   if (ieee154e_isSynch() == FALSE || neighbors_getMyDAGrank() == DEFAULTDAGRANK) 
     return;
   
   // only run on sensor node
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] != SENSOR_ADDR) {
      opentimers_stop(sixtop_light_vars.timerId);
      return;
   }

   uint16_t lux = 0;
   if (sensors_is_present(SENSOR_ADC_TOTAL_SOLAR))
   {
      sixtop_light_read_cb = sensors_getCallbackRead(SENSOR_ADC_TOTAL_SOLAR);
      lux = sixtop_light_read_cb();
   }
       
       //detect if light is off
    //   if (lux < LUX_THRESHOLD)
    //     return;

   sixtop_light_vars.counter++;
   
   for (i = 0; i < 3; i++)
   {
       // get a free packet buffer
       pkt = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
       if (pkt==NULL) {
          openserial_printError(
             COMPONENT_LIGHT,
             ERR_NO_FREE_PACKET_BUFFER,
             (errorparameter_t)0,
             (errorparameter_t)0
          );
          return;
       }
       
       pkt->owner                         = COMPONENT_LIGHT;
       pkt->creator                       = COMPONENT_LIGHT;

       // payload
       packetfunctions_reserveHeaderSize(pkt,sizeof(uint32_t));
       *((uint16_t*)&pkt->payload[0]) = sixtop_light_vars.counter;
       *((uint16_t*)&pkt->payload[2]) = neighbors_getMyDAGrank();
       
       pkt->l2_nextORpreviousHop.type        = ADDR_16B;
       pkt->l2_nextORpreviousHop.addr_16b[0] = 0xff;
       pkt->l2_nextORpreviousHop.addr_16b[1] = 0xff;

#if DEBUG == TRUE    
       openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_SEND,
                         (errorparameter_t)(sixtop_light_vars.counter),
                         (errorparameter_t)neighbors_getMyDAGrank());
#endif
       
       if ((sixtop_send(pkt))==E_FAIL) {
          openqueue_freePacketBuffer(pkt);
       }
   }
}