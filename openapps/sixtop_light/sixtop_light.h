#ifndef __SIXTOP_LIGHT_H
#define __SIXTOP_LIGHT_H

#include "opentimers.h"

//=========================== define ==========================================

#define SIXTOP_LIGHT_CANCEL_MS     1000
#define SIXTOP_LIGHT_SEND_MS       1
#define LUX_THRESHOLD              2000

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   opentimer_id_t       cancelTimerId;  ///< periodic timer which triggers when to cancel the processing
   opentimer_id_t       sendTimerId;    ///< periodic timer which triggers when to transmit      
   int16_t              counter;        ///< incrementing counter which is written into the packet
   bool                 processing;
   uint16_t             lux;
} sixtop_light_vars_t;

//=========================== prototypes ======================================

void sixtop_light_init(void);
void sixtop_light_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void sixtop_light_receive(OpenQueueEntry_t* msg);
bool sixtop_light_is_processing(void);
void sixtop_light_send(uint16_t lux);

#endif
