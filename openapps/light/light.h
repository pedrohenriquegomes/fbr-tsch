#ifndef __LIGHT_H
#define __LIGHT_H

#include "opentimers.h"

//=========================== define ===========================================

#define LIGHT_SEND_PERIOD_MS      100
#define LIGHT_SEND_RETRIES        5
#define LUX_THRESHOLD             500
#define LUX_HYSTERESIS            100

//=== hardcoded addresses (last 2 bytes of the EUI64)

// USC
/*
#define SINK_ID                   0xed4e
#define SENSOR_ID                 0x89a5
*/
// Inria
#define SINK_ID                   0x6f16
#define SENSOR_ID                 0xb957

//=========================== typedef ==========================================

//=========================== variables ========================================

typedef struct {
   opentimer_id_t       sendTimerId;        // timer ID for sending multiple packets in every event
   opentimer_id_t       fwTimerId;          // timer ID for forwarding one packet
   uint16_t             seqnum;             // event sequence number
   uint16_t             light_reading;      // current light sensor reading
   bool                 light_state;        // current state of the light (TRUE==on, FALSE==off)
   bool                 busyForwarding;     // I'm busy forwarding a packet
   uint8_t              n_tx;               // controls the number of packets transmitted in each event
   uint8_t              received_asn[5];    // holds the ASN of last event
   OpenQueueEntry_t*    pktToForward;       // packet to forward
} light_vars_t;

//=========================== prototypes =======================================

// initialization
void     light_init(void);
void     light_trigger(void);
bool     light_checkMyId(uint16_t addr);
bool     light_get_light_state(void);
uint16_t light_get_seqnum(void);
void     light_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void     light_receive_data(OpenQueueEntry_t* msg);
void     light_receive_beacon(OpenQueueEntry_t* msg);

#endif
