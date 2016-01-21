#ifndef __LIGHT_H
#define __LIGHT_H

#include "opentimers.h"

//=========================== define ==========================================

#define LIGHT_SEND_PERIOD_MS       100
#define LIGHT_SEND_RETRIES         3
//#define LUX_THRESHOLD              1000
#define LUX_THRESHOLD              85

//#define SINK_ID             0xed4f
#define SINK_ID             0x13cf
//#define SENSOR_ID           0xecbf
#define SENSOR_ID           0x5a53

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   opentimer_id_t       sendTimerId;    // timer ID for sending multiple packets in every event
   int16_t              counter;        // event sequence number
   uint16_t             lux;            // current lux read
   bool                 state;          // current state
   bool                 initialized;    // flag to indicate the application has been initialized
   uint8_t              n_tx;           // controls the number of packets transmitted in each event
} light_vars_t;

//=========================== prototypes ======================================

void light_init();
void light_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void light_receive_data(OpenQueueEntry_t* msg);
void light_receive_beacon(OpenQueueEntry_t* msg);
void light_send(uint16_t lux, bool state);
void light_initialize(bool state);
bool light_is_initialized(void);
bool light_state(void);
uint16_t light_counter(void);
bool light_checkMyId(uint16_t addr);
void light_tx_packet(OpenQueueEntry_t* pkt, uint16_t counter, bool state);

#endif
