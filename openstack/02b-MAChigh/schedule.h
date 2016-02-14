#ifndef __SCHEDULE_H
#define __SCHEDULE_H

/**
\addtogroup MAChigh
\{
\addtogroup Schedule
\{
*/

#include "opendefs.h"

//=========================== define ==========================================

/**
\brief The length of the superframe, in slots.

The superframe repears over time and can be arbitrarly long.
*/
#define NUM_EB_SLOTS         1
#define NUM_TXRX_SLOTS       10
#define SLOTFRAME_LENGTH     13

#define MAXACTIVESLOTS       (NUM_EB_SLOTS+NUM_TXRX_SLOTS)

//=========================== typedef =========================================

typedef uint8_t    channelOffset_t;
typedef uint16_t   slotOffset_t;
typedef uint16_t   frameLength_t;

typedef enum {
   CELLTYPE_OFF              = 0,
   CELLTYPE_EB               = 1,
   CELLTYPE_TXRX             = 2,
} cellType_t;

typedef struct {
   slotOffset_t    slotOffset;
   uint8_t         channelOffset;
   cellType_t      type;
   uint8_t         numRx;
   uint8_t         numTx;
   asn_t           lastUsedAsn;
   void*           next;
} scheduleEntry_t;

BEGIN_PACK
typedef struct {
   uint8_t         row;
   slotOffset_t    slotOffset;
   uint8_t         channelOffset;
   uint8_t         type;
   uint8_t         numRx;
   uint8_t         numTx;
   asn_t           lastUsedAsn;
} debugScheduleEntry_t;
END_PACK

//=========================== variables =======================================

typedef struct {
   scheduleEntry_t  scheduleBuf[MAXACTIVESLOTS];
   scheduleEntry_t* currentScheduleEntry;
   uint8_t          debugPrintRow;
} schedule_vars_t;

//=========================== prototypes ======================================

// admin
void               schedule_init(void);
bool               debugPrint_schedule(void);

// from 6top
owerror_t          schedule_addActiveSlot(
   slotOffset_t         slotOffset,
   uint8_t              channelOffset,
   cellType_t           type
);

// from IEEE802154E
void               schedule_syncSlotOffset(slotOffset_t targetSlotOffset);
void               schedule_advanceSlot(void);
slotOffset_t       schedule_getNextActiveSlotOffset(void);
cellType_t         schedule_getType(void);
channelOffset_t    schedule_getChannelOffset(void);

void               schedule_indicateRx(
   asn_t*   asnTimestamp
);
void               schedule_indicateTx(
   asn_t*    asnTimestamp,
   bool      unused
);

/**
\}
\}
*/
          
#endif
