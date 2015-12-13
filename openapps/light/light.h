#ifndef __LIGHT_H
#define __LIGHT_H

#include "opentimers.h"

//=========================== define ==========================================

//#define LIGHT_SEND_MS       1
#define LIGHT_PROC_MS       1000
#define LUX_THRESHOLD       1000

#define SINK_ID             0xed4f
#define SENSOR_ID           0xecbf

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
//   opentimer_id_t       cancelTimerId;  ///< periodic timer which triggers when to cancel the processing
//   opentimer_id_t       sendTimerId;    ///< periodic timer which triggers when to transmit      
   int16_t              counter;        ///< incrementing counter which is written into the packet
   uint16_t             lux;
   bool                 state;
   bool                 initialized;
   bool                 processing;
} light_vars_t;

//=========================== prototypes ======================================

void light_init();
void light_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void light_receive(OpenQueueEntry_t* msg);
void light_send(uint16_t lux, bool state);
void light_initialize(bool state);
bool light_is_initialized(void);
bool light_state(void);
bool light_checkMyId(uint16_t addr);
                   
#endif
