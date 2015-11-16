#ifndef __UDP_LIGHT_H
#define __UDP_LIGHT_H

#include "opentimers.h"

//=========================== define ==========================================

#define UDP_LIGHT_PERIOD_MS     1000
#define UDP_LIGHT_ON            0xAA

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   opentimer_id_t       timerId;  ///< periodic timer which triggers transmission
   uint16_t             status;   ///< incrementing counter which is written into the packet
} udp_light_vars_t;

//=========================== prototypes ======================================

void udp_light_init(void);
void udp_light_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void udp_light_receive(OpenQueueEntry_t* msg);

#endif
