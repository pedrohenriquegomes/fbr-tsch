#ifndef __SIXTOP_LIGHT_H
#define __SIXTOP_LIGHT_H

#include "opentimers.h"

//=========================== define ==========================================

#define SIXTOP_LIGHT_PERIOD_MS     10000
#define LUX_THRESHOLD              600

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   opentimer_id_t       timerId;  ///< periodic timer which triggers transmission
   uint16_t             status;   ///< incrementing counter which is written into the packet
   int16_t              counter;
} sixtop_light_vars_t;

//=========================== prototypes ======================================

void sixtop_light_init(void);
void sixtop_light_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void sixtop_light_receive(OpenQueueEntry_t* msg);

#endif
