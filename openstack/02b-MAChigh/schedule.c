#include "opendefs.h"
#include "schedule.h"
#include "openserial.h"
#include "openrandom.h"
#include "packetfunctions.h"
#include "sixtop.h"
#include "idmanager.h"

//=========================== variables =======================================

schedule_vars_t schedule_vars;

//=========================== prototypes ======================================

void schedule_resetEntry(scheduleEntry_t* pScheduleEntry);

//=========================== public ==========================================

//=== admin

/**
\brief Initialize this module.

\post Call this function before calling any other function in this module.
*/
void schedule_init() {
   slotOffset_t    running_slotOffset;

   // reset local variables
   memset(&schedule_vars,0,sizeof(schedule_vars_t));
   for (running_slotOffset=0;running_slotOffset<MAXACTIVESLOTS;running_slotOffset++) {
      schedule_resetEntry(&schedule_vars.scheduleBuf[running_slotOffset]);
   }
   
   // EB slot(s)
   for (running_slotOffset=0;running_slotOffset<NUM_EB_SLOTS;running_slotOffset++) {
      schedule_addActiveSlot(
         running_slotOffset,      // slot offset
         0,                       // channel offset
         CELLTYPE_EB              // type of slot
      );
   }
   
   // TXRX slot(s)
   for (;running_slotOffset<NUM_EB_SLOTS+NUM_TXRX_SLOTS;running_slotOffset++) {
      schedule_addActiveSlot(
         running_slotOffset,      // slot offset
         0,                       // channel offset
         CELLTYPE_TXRX            // type of slot
      );
   }
}

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_schedule() {
   debugScheduleEntry_t temp;
   
   // increment the row just printed
   schedule_vars.debugPrintRow         = (schedule_vars.debugPrintRow+1)%MAXACTIVESLOTS;
   
   // gather status data
   temp.row                            = schedule_vars.debugPrintRow;
   temp.slotOffset                     = \
      schedule_vars.scheduleBuf[schedule_vars.debugPrintRow].slotOffset;
   temp.type                           = \
      schedule_vars.scheduleBuf[schedule_vars.debugPrintRow].type;
   temp.channelOffset                  = \
      schedule_vars.scheduleBuf[schedule_vars.debugPrintRow].channelOffset;
   temp.numRx                          = \
      schedule_vars.scheduleBuf[schedule_vars.debugPrintRow].numRx;
   temp.numTx                          = \
      schedule_vars.scheduleBuf[schedule_vars.debugPrintRow].numTx;
   memcpy(
      &temp.lastUsedAsn,
      &schedule_vars.scheduleBuf[schedule_vars.debugPrintRow].lastUsedAsn,
      sizeof(asn_t)
   );
   
   // send status data over serial port
   openserial_printStatus(
      STATUS_SCHEDULE,
      (uint8_t*)&temp,
      sizeof(debugScheduleEntry_t)
   );
   
   return TRUE;
}

//=== from 6top (writing the schedule)

/**
\brief Add a new active slot into the schedule.

\param slotOffset       The slotoffset of the new slot
\param type             The type of the cell
\param shared           Whether this cell is shared (TRUE) or not (FALSE).
\param channelOffset    The channelOffset of the new slot
\param neighbor         The neighbor associated with this cell (all 0's if
   none)
*/
owerror_t schedule_addActiveSlot(
      slotOffset_t    slotOffset,
      channelOffset_t channelOffset,
      cellType_t      type
   ) {
   scheduleEntry_t* slotContainer;
   scheduleEntry_t* previousSlotWalker;
   scheduleEntry_t* nextSlotWalker;
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   // find an empty schedule entry container
   slotContainer = &schedule_vars.scheduleBuf[0];
   while (
         slotContainer->type!=CELLTYPE_OFF &&
         slotContainer<=&schedule_vars.scheduleBuf[MAXACTIVESLOTS-1]
      ) {
      slotContainer++;
   }
   
   // abort it schedule overflow
   if (slotContainer>&schedule_vars.scheduleBuf[MAXACTIVESLOTS-1]) {
      ENABLE_INTERRUPTS();
      openserial_printCritical(
         COMPONENT_SCHEDULE,ERR_SCHEDULE_OVERFLOWN,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return E_FAIL;
   }
   
   // fill that schedule entry with parameters passed
   slotContainer->slotOffset                = slotOffset;
   slotContainer->channelOffset             = channelOffset;
   slotContainer->type                      = type;
   
   // insert in circular list
   if (schedule_vars.currentScheduleEntry==NULL) {
      // this is the first active slot added
      
      // the next slot of this slot is this slot
      slotContainer->next                   = slotContainer;
      
      // current slot points to this slot
      schedule_vars.currentScheduleEntry    = slotContainer;
   } else  {
      // this is NOT the first active slot added
      
      // find position in schedule
      previousSlotWalker                    = schedule_vars.currentScheduleEntry;
      while (1) {
         nextSlotWalker                     = previousSlotWalker->next;
         if (
               (
                     (previousSlotWalker->slotOffset <  slotContainer->slotOffset) &&
                     (slotContainer->slotOffset <  nextSlotWalker->slotOffset)
               )
               ||
               (
                     (previousSlotWalker->slotOffset <  slotContainer->slotOffset) &&
                     (nextSlotWalker->slotOffset <= previousSlotWalker->slotOffset)
               )
               ||
               (
                     (slotContainer->slotOffset <  nextSlotWalker->slotOffset) &&
                     (nextSlotWalker->slotOffset <= previousSlotWalker->slotOffset)
               )
         ) {
            break;
         }
         previousSlotWalker                 = nextSlotWalker;
      }
      // insert between previousSlotWalker and nextSlotWalker
      previousSlotWalker->next              = slotContainer;
      slotContainer->next                   = nextSlotWalker;
   }
   
   ENABLE_INTERRUPTS();
   return E_SUCCESS;
}

//=== from IEEE802154E: reading the schedule and updating statistics

void schedule_syncSlotOffset(slotOffset_t targetSlotOffset) {
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   while (schedule_vars.currentScheduleEntry->slotOffset!=targetSlotOffset) {
      schedule_advanceSlot();
   }
   
   ENABLE_INTERRUPTS();
}

/**
\brief advance to next active slot
*/
void schedule_advanceSlot() {
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   schedule_vars.currentScheduleEntry = schedule_vars.currentScheduleEntry->next;
   
   ENABLE_INTERRUPTS();
}

/**
\brief return slotOffset of next active slot
*/
slotOffset_t schedule_getNextActiveSlotOffset() {
   slotOffset_t res;   
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   res = ((scheduleEntry_t*)(schedule_vars.currentScheduleEntry->next))->slotOffset;
   
   ENABLE_INTERRUPTS();
   
   return res;
}

/**
\brief Get the type of the current schedule entry.

\returns The type of the current schedule entry.
*/
cellType_t schedule_getType() {
   cellType_t returnVal;
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   returnVal = schedule_vars.currentScheduleEntry->type;
   
   ENABLE_INTERRUPTS();
   
   return returnVal;
}

/**
\brief Get the channel offset of the current schedule entry.

\returns The channel offset of the current schedule entry.
*/
channelOffset_t schedule_getChannelOffset() {
   channelOffset_t returnVal;
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   returnVal = schedule_vars.currentScheduleEntry->channelOffset;
   
   ENABLE_INTERRUPTS();
   
   return returnVal;
}

/**
\brief Indicate the reception of a packet.
*/
void schedule_indicateRx(asn_t* asnTs) {
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   // increment usage statistics
   schedule_vars.currentScheduleEntry->numRx++;

   // update last used timestamp
   memcpy(&(schedule_vars.currentScheduleEntry->lastUsedAsn), asnTs, sizeof(asn_t));
   
   ENABLE_INTERRUPTS();
}

/**
\brief Indicate the transmission of a packet.
*/
void schedule_indicateTx(asn_t* asnTimestamp, bool unused) {
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   // increment usage statistics
   if (schedule_vars.currentScheduleEntry->numTx==0xFF) {
      schedule_vars.currentScheduleEntry->numTx/=2;
   }
   schedule_vars.currentScheduleEntry->numTx++;
   
   // update last used timestamp
   memcpy(&schedule_vars.currentScheduleEntry->lastUsedAsn, asnTimestamp, sizeof(asn_t));
   
   ENABLE_INTERRUPTS();
}

//=========================== private =========================================

/**
\pre This function assumes interrupts are already disabled.
*/
void schedule_resetEntry(scheduleEntry_t* e) {
   e->slotOffset             = 0;
   e->channelOffset          = 0;
   e->type                   = CELLTYPE_OFF;
   e->numRx                  = 0;
   e->numTx                  = 0;
   e->lastUsedAsn.bytes0and1 = 0;
   e->lastUsedAsn.bytes2and3 = 0;
   e->lastUsedAsn.byte4      = 0;
   e->next                   = NULL;
}
