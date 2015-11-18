#include "opendefs.h"
#include "sensors.h"
#include "icmp_light.h"
#include "neighbors.h"
#include "openqueue.h"
#include "openserial.h"
#include "opentimers.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "idmanager.h"
#include "IEEE802154E.h"
#include "icmpv6.h"

//=========================== variables =======================================

icmp_light_vars_t        icmp_light_vars;
callbackRead_cbt         icmp_light_read_cb;

static const uint8_t icmp_light_broadcast_addr[]   = {
   0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a
}; 

//=========================== prototypes ======================================

void icmp_light_timer_cb(opentimer_id_t id);
void icmp_light_task_cb(void);

//=========================== public ==========================================

void icmp_light_init() {
   
   // clear local variables
   memset(&icmp_light_vars,0,sizeof(icmp_light_vars_t));
   
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] == SENSOR_ADDR) 
   {
      icmp_light_vars.counter = 0;
   }
   else
   {
      icmp_light_vars.counter = -1;
   }
   
   // start periodic timer
   icmp_light_vars.timerId                    = opentimers_start(
      ICMP_LIGHT_PERIOD_MS,
      TIMER_PERIODIC,TIME_MS,
      icmp_light_timer_cb
   );
}

void icmp_light_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

void icmp_light_receive(OpenQueueEntry_t* pkt) {
   OpenQueueEntry_t* fw;
   
   // don't run if not synch or does not know the RANK yet
   if (ieee154e_isSynch() == FALSE || neighbors_getMyDAGrank() == DEFAULTDAGRANK)
   {
     openqueue_freePacketBuffer(pkt);
     return;
   }

   //toss icmp header
   packetfunctions_tossHeader(pkt,sizeof(ICMPv6_ht));
   
   // retrieve the counter
   int16_t counter = pkt->payload[0] | (pkt->payload[1] << 8);

   // retrieve the rank
   uint16_t rank = pkt->payload[2] | (pkt->payload[3] << 8);
   
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_RCV,
                 (errorparameter_t)counter,
                 (errorparameter_t)rank);
   
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] == ROOT_ADDR) {
     if (counter > icmp_light_vars.counter)
     {
       icmp_light_vars.counter = counter;
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
     
      // take ownership over reply
      fw->creator = COMPONENT_LIGHT;
      fw->owner   = COMPONENT_LIGHT;
     
      fw->l4_protocol                   = IANA_ICMPv6;
      fw->l4_sourcePortORicmpv6Type     = IANA_ICMPv6_LIGHT;
      
      // copy the light value
      packetfunctions_reserveHeaderSize(fw,sizeof(uint32_t));
      *((uint16_t*)&fw->payload[0]) = counter;
      *((uint16_t*)&fw->payload[2]) = neighbors_getMyDAGrank();
      
      // Send a broadcast message
      fw->l3_destinationAdd.type        = ADDR_128B;
      memcpy(&fw->l3_destinationAdd.addr_128b[0],icmp_light_broadcast_addr,16);
   
      packetfunctions_reserveHeaderSize(fw,sizeof(ICMPv6_ht));
      ((ICMPv6_ht*)(fw->payload))->type         = fw->l4_sourcePortORicmpv6Type;
      ((ICMPv6_ht*)(fw->payload))->code         = 0;
   
      packetfunctions_calculateChecksum(fw,(uint8_t*)&(((ICMPv6_ht*)(fw->payload))->checksum));//do last
   
      openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_FW,
                 (errorparameter_t)counter,
                 (errorparameter_t)neighbors_getMyDAGrank());
            
      if ((icmpv6_send(fw))==E_FAIL) {
         openqueue_freePacketBuffer(fw);
      }
   }
         
   openqueue_freePacketBuffer(pkt);
}

//=========================== private =========================================

void icmp_light_timer_cb(opentimer_id_t id){
   
   scheduler_push_task(icmp_light_task_cb, TASKPRIO_COAP);
}

void icmp_light_task_cb() {
   OpenQueueEntry_t*    pkt;
   
   // don't run if not synch or does not know the RANK yet
   if (ieee154e_isSynch() == FALSE || neighbors_getMyDAGrank() == DEFAULTDAGRANK) 
     return;
   
   // only run on sensor node
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] != SENSOR_ADDR) {
      opentimers_stop(icmp_light_vars.timerId);
      return;
   }
   
   uint16_t lux = 0;
   if (sensors_is_present(SENSOR_LIGHT))
   {
      icmp_light_read_cb = sensors_getCallbackRead(SENSOR_LIGHT);
      lux = icmp_light_read_cb();
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
   pkt->l4_protocol                   = IANA_ICMPv6;
   pkt->l4_sourcePortORicmpv6Type     = IANA_ICMPv6_LIGHT;
   
   // Send a broadcast message
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],icmp_light_broadcast_addr,16);
         
   //ICMPv6 header
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint32_t));
   *((uint16_t*)&pkt->payload[0]) = icmp_light_vars.counter++;
   *((uint16_t*)&pkt->payload[2]) = neighbors_getMyDAGrank();
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(ICMPv6_ht));
   ((ICMPv6_ht*)(pkt->payload))->type         = pkt->l4_sourcePortORicmpv6Type;
   ((ICMPv6_ht*)(pkt->payload))->code         = 0;
      
   packetfunctions_calculateChecksum(pkt,(uint8_t*)&(((ICMPv6_ht*)(pkt->payload))->checksum));//do last
   
    openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_SEND,
                     (errorparameter_t)(icmp_light_vars.counter - 1),
                     (errorparameter_t)neighbors_getMyDAGrank());
      
   if ((icmpv6_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}