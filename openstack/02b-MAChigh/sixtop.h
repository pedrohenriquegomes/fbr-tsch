#ifndef __SIXTOP_H
#define __SIXTOP_H

/**
\addtogroup MAChigh
\{
\addtogroup sixtop
\{
*/

#include "opentimers.h"
#include "opendefs.h"
#include "processIE.h"
//=========================== define ==========================================

#define SIX2SIX_TIMEOUT_MS 4000
#define SIXTOP_MINIMAL_EBPERIOD 5 // minist period of sending EB

//=========================== module variables ================================

typedef struct {
   uint16_t             periodMaintenance;
   bool                 busySendingEB;           // TRUE when busy sending an enhanced beacon
   uint8_t              dsn;                     // current data sequence number
   uint8_t              mgtTaskCounter;          // counter to determine what management task to do
   opentimer_id_t       maintenanceTimerId;
   uint16_t             ebPeriod;                // period of sending EB
} sixtop_vars_t;

//=========================== prototypes ======================================

// admin
void      sixtop_init(void);
void      sixtop_setEBPeriod(uint8_t ebPeriod);
void      sixtop_multiplyEBPeriod(uint8_t factor);
void      sixtop_addEBPeriod(uint8_t factor);
// from upper layer
owerror_t sixtop_send(OpenQueueEntry_t *msg);
// from lower layer
void      task_sixtopNotifSendDone(void);
void      task_sixtopNotifReceive(void);
/**
\}
\}
*/

#endif
