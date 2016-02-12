#ifndef __LIGHT_H
#define __LIGHT_H

#include "opentimers.h"

//=========================== define ===========================================

// application-level switches
#define LIGHT_PRINTOUT_READING
#define LIGHT_CALCULATE_DELAY
#define LIGHT_FAKESEND

// defines
#define LIGHT_FAKESEND_PERIOD     400 // period, in slots, of sending data
#define LIGHT_BURSTSIZE             3 // number of packets sent on each light event
#define LUX_THRESHOLD             500
#define LUX_HYSTERESIS            100

//=== hardcoded addresses (last 2 bytes of the EUI64)

/*
// Pedro@USC
#define SINK_ID                   0xed4e
#define SENSOR_ID                 0x89a5
*/
// Thomas@Inria
#define SINK_ID                   0x6f16
#define SENSOR_ID                 0xb957
/*
// Thomas@home
#define SINK_ID                   0xbb5e
#define SENSOR_ID                 0x930f
*/

//=========================== typedef ==========================================

BEGIN_PACK
typedef struct {                                 // always written big endian, i.e. MSB in addr[0]
   uint16_t  type;
   uint16_t  src;
   uint16_t  seqnum;
   uint8_t   light_state;
} light_ht;
END_PACK

//=========================== variables ========================================

typedef struct {
   // app state
   uint16_t             burstId;            // current burst ID (identifying the event)
   uint16_t             pktId;              // current packet ID (identifying the packet in a burst)
   uint8_t              pktIDMap;           // each flag is set when the corresponding pktId was sent
   uint16_t             light_reading;      // current light sensor reading
   bool                 light_state;        // current state of the light (TRUE==on, FALSE==off)
   asn_t                lastEventAsn;       // holds the ASN of last event
   // timers
   opentimer_id_t       fwdTimerId;         // timer ID for forwarding one packet
   // sending
   uint8_t              numBurstPktsSent;   // controls the number of packets transmitted in each event
} light_vars_t;

//=========================== prototypes =======================================

// initialization
void     light_init(void);
void     light_trigger(void);
void     light_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void     light_receive_data(OpenQueueEntry_t* msg);
void     light_receive_beacon(OpenQueueEntry_t* msg);

#endif
