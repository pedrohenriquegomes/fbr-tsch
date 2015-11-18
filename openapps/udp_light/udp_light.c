#include "opendefs.h"
#include "sensors.h"
#include "udp_light.h"
#include "neighbors.h"
#include "openudp.h"
#include "openqueue.h"
#include "opentimers.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "IEEE802154E.h"
#include "idmanager.h"

//=========================== variables =======================================

udp_light_vars_t        udp_light_vars;
callbackRead_cbt        udp_light_read_cb;

//static const uint8_t udp_light_dst_addr[]   = {
//   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
//}; 

static const uint8_t udp_light_dst_addr[]   = {
   0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a
}; 

//=========================== prototypes ======================================

void udp_light_timer_cb(opentimer_id_t id);
void udp_light_task_cb(void);

//=========================== public ==========================================

void udp_light_init() {
   
   // clear local variables
   memset(&udp_light_vars,0,sizeof(udp_light_vars_t));
   
   udp_light_vars.counter = -1;
   
   // start periodic timer
   udp_light_vars.timerId                    = opentimers_start(
      UDP_LIGHT_PERIOD_MS,
      TIMER_PERIODIC,TIME_MS,
      udp_light_timer_cb
   );
}

void udp_light_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

void udp_light_receive(OpenQueueEntry_t* pkt) {
   OpenQueueEntry_t* fw;
   
  // don't run if not synch or does not know the RANK yet
   if (ieee154e_isSynch() == FALSE || neighbors_getMyDAGrank() == DEFAULTDAGRANK)
   {
     openqueue_freePacketBuffer(pkt);
     return;
   }

   // retrieve the counter
   int16_t counter = pkt->payload[0] | (pkt->payload[1] << 8);

   // retrieve the rank
   uint16_t rank = pkt->payload[2] | (pkt->payload[3] << 8);
   
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_RCV,
           (errorparameter_t)counter,
           (errorparameter_t)rank);
    
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] == ROOT_ADDR) {
     if (counter > udp_light_vars.counter)
     {
       udp_light_vars.counter = counter;
     }
     openqueue_freePacketBuffer(pkt);
     return;
   }
   
   //check if rank is greater than ours
   if (rank > neighbors_getMyDAGrank())
   {
      pkt->owner = COMPONENT_LIGHT;
      
      // get a new openqueuEntry_t for the retransmission
      fw = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
      if (fw==NULL) {
        openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,
                              (errorparameter_t)1,
                              (errorparameter_t)0);
        openqueue_freePacketBuffer(fw);
        return;
      }
     
      fw->owner                         = COMPONENT_LIGHT;
      fw->creator                       = COMPONENT_LIGHT;
      fw->l4_protocol                   = IANA_UDP;
      fw->l4_destination_port           = WKP_UDP_LIGHT;
      fw->l4_sourcePortORicmpv6Type     = WKP_UDP_LIGHT;
      fw->l3_destinationAdd.type        = ADDR_128B;
      memcpy(&fw->l3_destinationAdd.addr_128b[0],udp_light_dst_addr,16);
   
      packetfunctions_reserveHeaderSize(fw,pkt->length);
      *((uint16_t*)&fw->payload[0]) = counter;
      *((uint16_t*)&fw->payload[2]) = neighbors_getMyDAGrank();
   
      openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_FW,
                       (errorparameter_t)counter,
                       (errorparameter_t)neighbors_getMyDAGrank());
   
      if ((openudp_send(fw))==E_FAIL) {
        openqueue_freePacketBuffer(fw);
      }
   }
   
   openqueue_freePacketBuffer(pkt);
   
}

//=========================== private =========================================

void udp_light_timer_cb(opentimer_id_t id){
   
   scheduler_push_task(udp_light_task_cb,TASKPRIO_COAP);
}

void udp_light_task_cb() {
   OpenQueueEntry_t*    pkt;
   
   // don't run if not synch or does not know the RANK yet
   if (ieee154e_isSynch() == FALSE || neighbors_getMyDAGrank() == DEFAULTDAGRANK) 
     return;
   
   // only run on sensor node
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] != SENSOR_ADDR) {
      opentimers_stop(udp_light_vars.timerId);
      return;
   }
   
   uint16_t lux = 0;
   if (sensors_is_present(SENSOR_LIGHT))
   {
      udp_light_read_cb = sensors_getCallbackRead(SENSOR_LIGHT);
      lux = udp_light_read_cb();
   }

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
   pkt->l4_protocol                   = IANA_UDP;
   pkt->l4_destination_port           = WKP_UDP_LIGHT;
   pkt->l4_sourcePortORicmpv6Type     = WKP_UDP_LIGHT;
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],udp_light_dst_addr,16);
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint32_t));
   *((uint16_t*)&pkt->payload[0]) = udp_light_vars.counter++;
   *((uint16_t*)&pkt->payload[2]) = neighbors_getMyDAGrank();
   
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_SEND,
                       (errorparameter_t)(udp_light_vars.counter - 1),
                       (errorparameter_t)neighbors_getMyDAGrank());
  
   if ((openudp_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}