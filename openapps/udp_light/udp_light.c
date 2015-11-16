#include "opendefs.h"
#include "sensors.h"
#include "udp_light.h"
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

static const uint8_t udp_light_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
}; 

//=========================== prototypes ======================================

void udp_light_timer_cb(opentimer_id_t id);
void udp_light_task_cb(void);

//=========================== public ==========================================

void udp_light_init() {
   
   // clear local variables
   memset(&udp_light_vars,0,sizeof(udp_light_vars_t));
   
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
   
   openqueue_freePacketBuffer(pkt);
   
   openserial_printError(
      COMPONENT_UINJECT,
      ERR_RCVD_ECHO_REPLY,
      (errorparameter_t)0,
      (errorparameter_t)0
   );
}

//=========================== private =========================================

void udp_light_timer_cb(opentimer_id_t id){
   
   scheduler_push_task(udp_light_task_cb,TASKPRIO_COAP);
}

void udp_light_task_cb() {
   OpenQueueEntry_t*    pkt;
   
   // don't run if not synch
   if (ieee154e_isSynch() == FALSE) return;
   
   // only run on sensor node
   if (idmanager_getMyID(ADDR_64B)->addr_64b[7] != SENSOR_ADD) {
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
   pkt = openqueue_getFreePacketBuffer(COMPONENT_UDP_LIGHT);
   if (pkt==NULL) {
      openserial_printError(
         COMPONENT_UINJECT,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   pkt->owner                         = COMPONENT_UDP_LIGHT;
   pkt->creator                       = COMPONENT_UDP_LIGHT;
   pkt->l4_protocol                   = IANA_UDP;
   pkt->l4_destination_port           = WKP_UDP_LIGHT;
   pkt->l4_sourcePortORicmpv6Type     = WKP_UDP_LIGHT;
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],udp_light_dst_addr,16);
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint16_t));
   *((uint16_t*)&pkt->payload[0]) = lux;
   
   
   if ((openudp_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}