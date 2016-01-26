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

opentimer_id_t      test_timer;
void test_timer_cb(opentimer_id_t id);

//=========================== prototypes ======================================

void light_timer_cb(opentimer_id_t id);
void light_send_task_cb(void);

//=========================== procedures ======================================

void light_init() 
{  
   // clear local variables
   memset(&light_vars,0,sizeof(light_vars_t));
  
#if THRESHOLD_TEST == TRUE
   
   // printout the current lux as a way of finding out the correct thresholds
   //if (light_checkMyId(SENSOR_ID) && sensors_is_present(SENSOR_LIGHT))
   if (light_checkMyId(SENSOR_ID) && sensors_is_present(SENSOR_ADC_TOTAL_SOLAR))
   {
      callbackRead_cbt             light_read_cb;
      uint16_t                     lux = 0;
      
      //light_read_cb = sensors_getCallbackRead(SENSOR_LIGHT);
      light_read_cb = sensors_getCallbackRead(SENSOR_ADC_TOTAL_SOLAR);
      lux = light_read_cb();
      
      openserial_printInfo(COMPONENT_LIGHT,ERR_LIGHT_THRESHOLD,
                          (errorparameter_t)lux,
                          0);
   }
#endif
   
   debugpins_user1_clr();
   
//  test_timer = opentimers_start(
//      1000,
//      TIMER_PERIODIC,
//      TIME_MS,
//      test_timer_cb
//   );
}

void test_timer_cb(opentimer_id_t id) {
    debugpins_user1_toggle();
}

void light_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

port_INLINE bool light_is_initialized(void)
{
  return light_vars.initialized;
}

port_INLINE void light_initialize(bool init)
{
  light_vars.initialized = init;
}

port_INLINE bool light_state(void)
{
  return light_vars.state;
}

port_INLINE uint16_t light_counter(void)
{
  return light_vars.counter;
}

// send a few packets. Uses a timer to spread the transmissions
void light_send(uint16_t lux, bool state)
{
  
  light_vars.lux = lux;
  light_vars.state = state;
  
  // start timer for additional packets
  light_vars.sendTimerId = opentimers_start(
      LIGHT_SEND_PERIOD_MS,
      TIMER_PERIODIC,
      TIME_MS,
      light_timer_cb
   );
}

// fires the multiples transmissions
void light_timer_cb(opentimer_id_t id){
  
  if (light_vars.n_tx < LIGHT_SEND_RETRIES)
  {
    scheduler_push_task(light_send_task_cb, TASKPRIO_MAX);
    light_vars.n_tx++;
    
  }
  else
  {
    opentimers_stop(id);
    light_vars.n_tx = 0;
  }
}

// receive a data packet and analyse it
void light_receive_data(OpenQueueEntry_t* pkt) 
{
   OpenQueueEntry_t* fw;
   int16_t counter;
   bool state;
   
   // don't run if not synched
   if (ieee154e_isSynch() == FALSE)
   {
     openqueue_freePacketBuffer(pkt);
     return;
   }
   
   // ownserhip
   pkt->owner = COMPONENT_LIGHT;
   
   // retrieve the counter
   counter = pkt->payload[0] | (pkt->payload[1] << 8);

   // retrieve the state
   state = (bool)pkt->payload[2];
     
#if DEBUG == TRUE
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_RCV,
                       (errorparameter_t)counter,
                       (errorparameter_t)state);
#endif     
   
   // free the packet
   openqueue_freePacketBuffer(pkt);
   
   // drop if we already received this packet
   if (counter <= light_vars.counter)
   {
     // if I receive many packets with sequence number much smaller, the sensor node rebooted, I need to reet my counter
//     uint8_t diff = light_vars.counter - counter;
//     if (diff > LARGE_SEQUENCE_NUM_DIFF)
//     {
//       if (light_vars.n_large_seq_num++ > 3)
//       {
//         light_vars.counter = 0;
//       }
//     }
     
#if DEBUG == TRUE
     openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_DROP,
                         (errorparameter_t)counter,
                         (errorparameter_t)state);
#endif     
     return;
   }
   
   // update the counter 
   light_vars.counter = counter;
   
   // update the state
   light_vars.state = state;
   
   // if I am the sink, process the message (update the state)
   if (light_checkMyId(SINK_ID)) 
   {  
     
     if (light_vars.state)
     {
      debugpins_user1_set();
     }
     else
     {
       debugpins_user1_clr();
     }
     
     /* TURN ON/OFF THE LIGHT AT THE SINK */
     openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_STATE,
                         (errorparameter_t)state,
                         (errorparameter_t)0);
     return;
   }
   
   // if I am not the sink, lets forward
   fw = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
   if (fw==NULL) {
     openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,0,0);
     return;
   }
   light_tx_packet(fw, counter, state);
   
#if DEBUG == TRUE
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_FW,
                       (errorparameter_t)counter,
                       (errorparameter_t)state);
#endif
}

// receive a beacon packet and analyse it
void light_receive_beacon(OpenQueueEntry_t* pkt) 
{
   OpenQueueEntry_t* fw;
   int16_t counter;
   bool state;
   
   // don't run if not synched
   if (ieee154e_isSynch() == FALSE)
   {
     openqueue_freePacketBuffer(pkt);
     return;
   }
   
   // ownserhip
   pkt->owner = COMPONENT_LIGHT;
   
   // retrieve the counter
   counter = pkt->l2_floodingCounter;

   // retrieve the state
   state = pkt->l2_floodingState;
   
   // update my info and drop if the beacon has a more recent counter
   if (counter >= light_vars.counter)
   {
     light_vars.counter = counter;
     light_vars.state = state;
  
     // if I am the sink, process the beacon (update the state)
     if (light_checkMyId(SINK_ID)) 
     {  
     
       if (light_vars.state)
       {
          debugpins_user1_set();
       }
       else
       {
         debugpins_user1_clr();
       }
     
       /* TURN ON/OFF THE LIGHT AT THE SINK */
       openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_STATE,
                           (errorparameter_t)state,
                           (errorparameter_t)0);
     }
     
     return;
   }
   
   // if I received a beacon that is older, but has the same state it means the other node is not out-of-date
   // if the other node is not closer to the sink, also discard the packet
   if ((light_vars.state == state) || (pkt->l2_rank >= neighbors_getMyDAGrank()))
   {
     return;
   }
   
   // if I received a beacon that is older from a node closer to the sink
   // and the node is out-of-date, we should update it asap
   fw = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
   if (fw==NULL) {
     openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,0,0);
     return;
   }
   light_tx_packet(fw, light_vars.counter, light_vars.state);
   
#if DEBUG == TRUE
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_GEN,
                       (errorparameter_t)light_vars.counter,
                       (errorparameter_t)light_vars.state);
#endif
}

// send a new packet when a state change happens
void light_send_task_cb() 
{
   OpenQueueEntry_t*    pkt;
   
   // increment the counter
   light_vars.counter++;
      
   // get a free packet buffer
   pkt = openqueue_getFreePacketBuffer(COMPONENT_LIGHT);
   if (pkt==NULL) {
      openserial_printError(COMPONENT_LIGHT,ERR_NO_FREE_PACKET_BUFFER,0,0);
      return;
   }
   light_tx_packet(pkt, light_vars.counter, light_vars.state);
     
#if DEBUG == TRUE    
   openserial_printInfo(COMPONENT_LIGHT,ERR_FLOOD_SEND,
                       (errorparameter_t)light_vars.counter,
//                     (errorparameter_t)light_vars.lux,
                       (errorparameter_t)light_vars.state);
#endif

}

// send the packet to the lower layer (sixtop)
port_INLINE void light_tx_packet(OpenQueueEntry_t* pkt, uint16_t counter, bool state)
{
   pkt->owner                         = COMPONENT_LIGHT;
   pkt->creator                       = COMPONENT_LIGHT;

   // payload
   packetfunctions_reserveHeaderSize(pkt,3);
   *((uint8_t*)&pkt->payload[0]) = light_vars.counter & 0xff;
   *((uint8_t*)&pkt->payload[1]) = light_vars.counter >> 8;
   *((uint8_t*)&pkt->payload[2]) = state;
       
   pkt->l2_nextORpreviousHop.type        = ADDR_16B;
   pkt->l2_nextORpreviousHop.addr_16b[0] = 0xff;
   pkt->l2_nextORpreviousHop.addr_16b[1] = 0xff;
       
   if ((sixtop_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}

// check if my id is equal to addr
port_INLINE bool light_checkMyId(uint16_t addr)
{
  return ((idmanager_getMyID(ADDR_64B)->addr_64b[7] == (addr & 0xff)) &&
          (idmanager_getMyID(ADDR_64B)->addr_64b[6] == (addr >> 8)));
}
