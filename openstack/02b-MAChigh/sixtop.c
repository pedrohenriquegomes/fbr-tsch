#include "opendefs.h"
#include "sixtop.h"
#include "openserial.h"
#include "openqueue.h"
#include "neighbors.h"
#include "IEEE802154E.h"
//#include "iphc.h"
//#include "otf.h"
#include "packetfunctions.h"
#include "openrandom.h"
#include "scheduler.h"
#include "opentimers.h"
#include "debugpins.h"
#include "leds.h"
#include "processIE.h"
#include "IEEE802154.h"
#include "idmanager.h"
#include "schedule.h"
#include "light.h"

//=========================== variables =======================================

sixtop_vars_t sixtop_vars;

//=========================== prototypes ======================================

// send internal
owerror_t     sixtop_send_internal(
   OpenQueueEntry_t*    msg,
   bool                 payloadIEPresent
);

// timer interrupt callbacks
void          sixtop_maintenance_timer_cb(opentimer_id_t id);

//=== EB/KA task

void          timer_sixtop_management_fired(void);
void          sixtop_sendEB(void);

//=== six2six task

void          timer_sixtop_six2six_timeout_fired(void);
void          sixtop_six2six_sendDone(
   OpenQueueEntry_t*    msg,
   owerror_t            error
);
void          sixtop_notifyReceiveCommand(
   opcode_IE_ht*        opcode_ie, 
   bandwidth_IE_ht*     bandwidth_ie, 
   schedule_IE_ht*      schedule_ie,
   open_addr_t*         addr
);
void          sixtop_notifyReceiveLinkRequest(
   bandwidth_IE_ht*     bandwidth_ie,
   schedule_IE_ht*      schedule_ie,
   open_addr_t*         addr
);
void          sixtop_linkResponse(
   bool                 success,
   open_addr_t*         tempNeighbor,
   uint8_t              bandwidth,
   schedule_IE_ht*      schedule_ie
);
void          sixtop_notifyReceiveLinkResponse(
   bandwidth_IE_ht*     bandwidth_ie,
   schedule_IE_ht*      schedule_ie,
   open_addr_t*         addr
);
void          sixtop_notifyReceiveRemoveLinkRequest(
   schedule_IE_ht*      schedule_ie,
   open_addr_t*         addr
);

//=== helper functions

bool          sixtop_candidateAddCellList(
   uint8_t*             type,
   uint8_t*             frameID,
   uint8_t*             flag,
   cellInfo_ht*         cellList
);
bool          sixtop_candidateRemoveCellList(
   uint8_t*             type,
   uint8_t*             frameID,
   uint8_t*             flag,
   cellInfo_ht*         cellList,
   open_addr_t*         neighbor
);
void          sixtop_addCellsByState(
   uint8_t              slotframeID,
   uint8_t              numOfLinks,
   cellInfo_ht*         cellList,
   open_addr_t*         previousHop,
   uint8_t              state
);
void          sixtop_removeCellsByState(
   uint8_t              slotframeID,
   uint8_t              numOfLink,
   cellInfo_ht*         cellList,
   open_addr_t*         previousHop
);
bool          sixtop_areAvailableCellsToBeScheduled(
   uint8_t              frameID, 
   uint8_t              numOfCells, 
   cellInfo_ht*         cellList, 
   uint8_t              bandwidth
);

//=========================== public ==========================================

void sixtop_init() {
   
   //sixtop_vars.periodMaintenance  = 930 +(openrandom_get16b()&0x3f);
   sixtop_vars.periodMaintenance  = 60 +(openrandom_get16b()&0x3f);
   sixtop_vars.busySendingEB      = FALSE;
   sixtop_vars.dsn                = 0;
   sixtop_vars.mgtTaskCounter     = 0;
   sixtop_vars.ebPeriod           = EBPERIOD;
   
   sixtop_vars.maintenanceTimerId = opentimers_start(
      sixtop_vars.periodMaintenance,
      TIMER_PERIODIC,
      TIME_MS,
      sixtop_maintenance_timer_cb
   );
}

void sixtop_setEBPeriod(uint8_t ebPeriod) {
   sixtop_vars.ebPeriod = ebPeriod;
}

uint8_t      sixtop_getEBPeriod(void) {
  return sixtop_vars.ebPeriod;
}

void sixtop_setHandler(six2six_handler_t handler) {
    sixtop_vars.handler = handler;
}

void sixtop_multiplyEBPeriod(uint8_t factor)
{
  sixtop_vars.ebPeriod *= factor;
}

void sixtop_addEBPeriod(uint8_t factor)
{
  sixtop_vars.ebPeriod += factor;
}

//======= from upper layer

owerror_t sixtop_send(OpenQueueEntry_t *msg) {
   
   // set metadata
   msg->owner        = COMPONENT_SIXTOP;
   msg->l2_frameType = IEEE154_TYPE_DATA;

   msg->l2_rankPresent = FALSE;

   if (msg->l2_payloadIEpresent == FALSE) {
      return sixtop_send_internal(
         msg,
         FALSE
      );
   } else {
      return sixtop_send_internal(
         msg,
         TRUE
      );
   }
}

//======= from lower layer

void task_sixtopNotifSendDone() {
   OpenQueueEntry_t* msg;
   
   // get recently-sent packet from openqueue
   msg = openqueue_sixtopGetSentPacket();
   if (msg==NULL) {
      openserial_printCritical(
         COMPONENT_SIXTOP,
         ERR_NO_SENT_PACKET,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   // take ownership
   msg->owner = COMPONENT_SIXTOP;
   
   // update neighbor statistics
   if (msg->l2_sendDoneError==E_SUCCESS) {
      neighbors_indicateTx(
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         TRUE,
         &msg->l2_asn
      );
   } else {
      neighbors_indicateTx(
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         FALSE,
         &msg->l2_asn
      );
   }
   
   // send the packet to where it belongs
   switch (msg->creator) {
      
      case COMPONENT_SIXTOP:
         if (msg->l2_frameType==IEEE154_TYPE_BEACON) {
            // this is a EB
            
            // not busy sending EB anymore
            sixtop_vars.busySendingEB = FALSE;
         }
         // discard packets
         openqueue_freePacketBuffer(msg);
         
         // restart a random timer
         //sixtop_vars.periodMaintenance  = 930 +(openrandom_get16b()&0x3f);
         sixtop_vars.periodMaintenance  = 60 +(openrandom_get16b()&0x3f);
         opentimers_setPeriod(
            sixtop_vars.maintenanceTimerId,
            TIME_MS,
            sixtop_vars.periodMaintenance
         );
         break;
  
      case COMPONENT_LIGHT:
         light_sendDone(msg,msg->l2_sendDoneError);
         break;
         
      default:
         // send the rest up the stack
         break;
   }
}

void task_sixtopNotifReceive() {
   OpenQueueEntry_t* msg;
   uint16_t          lenIE;
   
   // get received packet from openqueue
   msg = openqueue_sixtopGetReceivedPacket();
   if (msg==NULL) {
      openserial_printCritical(
         COMPONENT_SIXTOP,
         ERR_NO_RECEIVED_PACKET,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   // take ownership
   msg->owner = COMPONENT_SIXTOP;
   
   // process the header IEs
   lenIE=0;
      
   // toss the header IEs
   packetfunctions_tossHeader(msg,lenIE);
   
   // update neighbor statistics
   neighbors_indicateRx(
      &(msg->l2_nextORpreviousHop),
      msg->l1_rssi,
      &msg->l2_asn,
      msg->l2_joinPriorityPresent,
      msg->l2_joinPriority
   );
   
   // reset it to avoid race conditions with this var.
   msg->l2_joinPriorityPresent = FALSE; 
   
   // send the packet up the stack, if it qualifies
   switch (msg->l2_frameType) {
      case IEEE154_TYPE_BEACON:
        // update the rank
         neighbors_indicateRxEB(msg);
         // if the beacon comes from an upper node we should check if the node is up-to-date
         if (msg->l2_rank < neighbors_getMyDAGrank())
         {
           light_receive_beacon(msg);
         } else {
            openqueue_freePacketBuffer(msg);
         }
         break;
      case IEEE154_TYPE_DATA:
        // we only have one type of data packet. It is from sixtop light application
        if (msg->length>0) {
           light_receive_data(msg);
         } else {
            // free up the RAM
            openqueue_freePacketBuffer(msg);
         }
        break;
      case IEEE154_TYPE_CMD:
      case IEEE154_TYPE_ACK:
      default:
         // free the packet's RAM memory
         openqueue_freePacketBuffer(msg);
         // log the error
         openserial_printError(
            COMPONENT_SIXTOP,
            ERR_MSG_UNKNOWN_TYPE,
            (errorparameter_t)msg->l2_frameType,
            (errorparameter_t)0
         );
         break;
   }
}

//======= debugging

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_myDAGrank() {
   uint16_t output;
   
   output = 0;
   
   output = neighbors_getMyDAGrank();
   openserial_printStatus(STATUS_DAGRANK,(uint8_t*)&output,sizeof(uint16_t));
   return TRUE;
}

//=========================== private =========================================

/**
\brief Transfer packet to MAC.

This function adds a IEEE802.15.4 header to the packet and leaves it the 
OpenQueue buffer. The very last thing it does is assigning this packet to the 
virtual component COMPONENT_SIXTOP_TO_IEEE802154E. Whenever it gets a change,
IEEE802154E will handle the packet.

\param[in] msg The packet to the transmitted
\param[in] iePresent Indicates wheter an Information Element is present in the
   packet.
\param[in] frameVersion The frame version to write in the packet.

\returns E_SUCCESS iff successful.
*/
owerror_t sixtop_send_internal(
   OpenQueueEntry_t* msg, 
   bool    payloadIEPresent) {

   // assign a number of retries
   if (
      packetfunctions_isBroadcastMulticast(&(msg->l2_nextORpreviousHop))==TRUE
      ) {
      msg->l2_retriesLeft = 1;
   } else {
      msg->l2_retriesLeft = TXRETRIES + 1;
   }
   // record this packet's dsn (for matching the ACK)
   msg->l2_dsn = sixtop_vars.dsn++;
   // this is a new packet which I never attempted to send
   msg->l2_numTxAttempts = 0;
   // transmit with the default TX power
   msg->l1_txPower = TX_POWER;
   // add a IEEE802.15.4 header
   ieee802154_prependHeader(msg,
                            msg->l2_frameType,
                            payloadIEPresent,
                            msg->l2_dsn,
                            &(msg->l2_nextORpreviousHop)
                            );
   // change owner to IEEE802154E fetches it from queue
   msg->owner  = COMPONENT_SIXTOP_TO_IEEE802154E;
   return E_SUCCESS;
}

// timer interrupt callbacks

void sixtop_maintenance_timer_cb(opentimer_id_t id) {
  if (++sixtop_vars.mgtTaskCounter == sixtop_vars.ebPeriod)
  {
    scheduler_push_task(timer_sixtop_management_fired,TASKPRIO_SIXTOP);
    sixtop_vars.mgtTaskCounter = 0;
  }   
}

//======= EB/KA task

/**
\brief Timer handlers which triggers MAC management task.

This function is called in task context by the scheduler after the RES timer
has fired. This timer is set to fire every second, on average.

The body of this function executes one of the MAC management task.
*/
void timer_sixtop_management_fired(void) {
    sixtop_sendEB();
}

/**
\brief Send an EB.

This is one of the MAC management tasks. This function inlines in the
timers_res_fired() function, but is declared as a separate function for better
readability of the code.
*/
port_INLINE void sixtop_sendEB() {
   OpenQueueEntry_t* eb;
   uint8_t len;
   
   len = 0;
   
   if ((ieee154e_isSynch()==FALSE) || (neighbors_getMyDAGrank()==DEFAULTDAGRANK)){
      // I'm not sync'ed or I did not acquire a DAGrank
      
      // delete packets genereted by this module (EB and KA) from openqueue
      openqueue_removeAllCreatedBy(COMPONENT_SIXTOP);
      
      // I'm now busy sending an EB
      sixtop_vars.busySendingEB = FALSE;
      
      // stop here
      return;
   }
   
   if (sixtop_vars.busySendingEB==TRUE) {
      // don't continue if I'm still sending a previous EB
      return;
   }
   
   // if I get here, I will send an EB
   
   // get a free packet buffer
   eb = openqueue_getFreePacketBuffer(COMPONENT_SIXTOP);
   if (eb==NULL) {
      openserial_printError(COMPONENT_SIXTOP,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      return;
   }
   
   // declare ownership over that packet
   eb->creator = COMPONENT_SIXTOP;
   eb->owner   = COMPONENT_SIXTOP;
   
   // reserve space for EB-specific header
   // reserving for IEs.
   len += processIE_prependCounterIE(eb);
   len += processIE_prependSyncIE(eb);
  
   // some l2 information about this packet
   eb->l2_frameType                     = IEEE154_TYPE_BEACON;
   eb->l2_nextORpreviousHop.type        = ADDR_16B;
   eb->l2_nextORpreviousHop.addr_16b[0] = 0xff;
   eb->l2_nextORpreviousHop.addr_16b[1] = 0xff;
   
   //I has an IE in my payload
   eb->l2_payloadIEpresent = TRUE;

   // lets embedd the Rank
   eb->l2_rankPresent     = TRUE;
   eb->l2_rank            = neighbors_getMyDAGrank();
     
   // put in queue for MAC to handle
   sixtop_send_internal(eb,eb->l2_payloadIEpresent);
   
   // I'm now busy sending an EB
   sixtop_vars.busySendingEB = TRUE;
}

//======= six2six task

void timer_sixtop_six2six_timeout_fired(void) {
   // timeout timer fired, reset the state of sixtop to idle
   sixtop_vars.six2six_state = SIX_IDLE;
   sixtop_vars.handler = SIX_HANDLER_NONE;
   opentimers_stop(sixtop_vars.timeoutTimerId);
}

void sixtop_six2six_sendDone(OpenQueueEntry_t* msg, owerror_t error){
   uint8_t i,numOfCells;
   uint8_t* ptr;
   cellInfo_ht cellList[SCHEDULEIEMAXNUMCELLS];
   
   memset(cellList,0,SCHEDULEIEMAXNUMCELLS*sizeof(cellInfo_ht));
  
   ptr = msg->l2_scheduleIE_cellObjects;
   numOfCells = msg->l2_scheduleIE_numOfCells;
   msg->owner = COMPONENT_SIXTOP_RES;
  
   if(error == E_FAIL) {
      sixtop_vars.six2six_state = SIX_IDLE;
      openqueue_freePacketBuffer(msg);
      return;
   }

   switch (sixtop_vars.six2six_state) {
      case SIX_WAIT_ADDREQUEST_SENDDONE:
         sixtop_vars.six2six_state = SIX_WAIT_ADDRESPONSE;
         break;
      case SIX_WAIT_ADDRESPONSE_SENDDONE:
         if (error == E_SUCCESS && numOfCells > 0){
             for (i=0;i<numOfCells;i++){
               //TimeSlot 2B
               cellList[i].tsNum       = *(ptr);
               cellList[i].tsNum      |= (*(ptr+1))<<8;
               //Ch.Offset 2B
               cellList[i].choffset    = *(ptr+2);
               cellList[i].choffset   |= (*(ptr+3))<<8;
               //LinkOption bitmap 1B
               cellList[i].linkoptions = *(ptr+4);
               ptr += 5;
             }
             sixtop_addCellsByState(
                 msg->l2_scheduleIE_frameID,
                 numOfCells,
                 cellList,
                 &(msg->l2_nextORpreviousHop),
                 sixtop_vars.six2six_state);
         }
         sixtop_vars.six2six_state = SIX_IDLE;
         break;
      case SIX_WAIT_REMOVEREQUEST_SENDDONE:
         if(error == E_SUCCESS && numOfCells > 0){
            for (i=0;i<numOfCells;i++){
               //TimeSlot 2B
               cellList[i].tsNum       = *(ptr);
               cellList[i].tsNum      |= (*(ptr+1))<<8;
               //Ch.Offset 2B
               cellList[i].choffset    = *(ptr+2);
               cellList[i].choffset   |= (*(ptr+3))<<8;
               //LinkOption bitmap 1B
               cellList[i].linkoptions = *(ptr+4);
               ptr += 5;
            }
            sixtop_removeCellsByState(
               msg->l2_scheduleIE_frameID,
               numOfCells,
               cellList,
               &(msg->l2_nextORpreviousHop)
            );
         }
         sixtop_vars.six2six_state = SIX_IDLE;
         opentimers_stop(sixtop_vars.timeoutTimerId);
         leds_debug_off();
         if (sixtop_vars.handler == SIX_HANDLER_MAINTAIN){
//             sixtop_addCells(&(msg->l2_nextORpreviousHop),1);
             sixtop_vars.handler = SIX_HANDLER_NONE;
         }
         break;
      default:
         //log error
         break;
   }
  
   // discard reservation packets this component has created
   openqueue_freePacketBuffer(msg);
}

void sixtop_notifyReceiveLinkResponse(
   bandwidth_IE_ht* bandwidth_ie, 
   schedule_IE_ht* schedule_ie,
   open_addr_t* addr){
   
   uint8_t bw,numOfcells,frameID;
  
   frameID = schedule_ie->frameID;
   numOfcells = schedule_ie->numberOfcells;
   bw = bandwidth_ie->numOfLinks;
  
   if(bw == 0){
      // link request failed
      // todo- should inform some one
      return;
   } else {
      // need to check whether the links are available to be scheduled.
      if(bw != numOfcells                                                ||
         schedule_ie->frameID != bandwidth_ie->slotframeID               ||
         sixtop_areAvailableCellsToBeScheduled(frameID, 
                                               numOfcells, 
                                               schedule_ie->cellList, 
                                               bw) == FALSE){
         // link request failed,inform uplayer
      } else {
         sixtop_addCellsByState(frameID,
                                bw,
                                schedule_ie->cellList,
                                addr,
                                sixtop_vars.six2six_state);
      // link request success,inform uplayer
      }
   }
   leds_debug_off();
   sixtop_vars.six2six_state = SIX_IDLE;
   sixtop_vars.handler = SIX_HANDLER_NONE;
  
   opentimers_stop(sixtop_vars.timeoutTimerId);
}

void sixtop_notifyReceiveRemoveLinkRequest(
   schedule_IE_ht* schedule_ie,
   open_addr_t* addr){
   
   uint8_t numOfCells,frameID;
   cellInfo_ht* cellList;
  
   numOfCells = schedule_ie->numberOfcells;
   frameID = schedule_ie->frameID;
   cellList = schedule_ie->cellList;
   
   leds_debug_on();
   
   sixtop_removeCellsByState(frameID,numOfCells,cellList,addr);
   
   if (sixtop_vars.handler == SIX_HANDLER_OTF) {
     // notify OTF
//     otf_notif_removedCell();
   } else {
       if (sixtop_vars.handler == SIX_HANDLER_MAINTAIN) {
           // if sixtop remove request handler is 
           sixtop_vars.handler = SIX_HANDLER_NONE;
       } else {
           // if any other handlers exist
       }
   }
   
   sixtop_vars.six2six_state = SIX_IDLE;

   leds_debug_off();
}

//======= helper functions

bool sixtop_candidateAddCellList(
      uint8_t*     type,
      uint8_t*     frameID,
      uint8_t*     flag,
      cellInfo_ht* cellList
   ){
   frameLength_t i;
   uint8_t counter;
   uint8_t numCandCells;
   
   *type = 1;
   *frameID = schedule_getFrameHandle();
   *flag = 1; // the cells listed in cellList are available to be schedule.
   
   numCandCells=0;
   for(counter=0;counter<SCHEDULEIEMAXNUMCELLS;counter++){
      i = openrandom_get16b()%schedule_getFrameLength();
      if(schedule_isSlotOffsetAvailable(i)==TRUE){
         cellList[numCandCells].tsNum       = i;
         cellList[numCandCells].choffset    = 0;
         cellList[numCandCells].linkoptions = CELLTYPE_TX;
         numCandCells++;
      }
   }
   
   if (numCandCells==0) {
      return FALSE;
   } else {
      return TRUE;
   }
}

bool sixtop_candidateRemoveCellList(
      uint8_t*     type,
      uint8_t*     frameID,
      uint8_t*     flag,
      cellInfo_ht* cellList,
      open_addr_t* neighbor
   ){
   uint8_t              i;
   uint8_t              numCandCells;
   slotinfo_element_t   info;
   
   *type           = 1;
   *frameID        = schedule_getFrameHandle();
   *flag           = 1;
  
   numCandCells    = 0;
   for(i=0;i<schedule_getMaxActiveSlots();i++){
      schedule_getSlotInfo(i,neighbor,&info);
      if(info.link_type == CELLTYPE_TX){
         cellList[numCandCells].tsNum       = i;
         cellList[numCandCells].choffset    = info.channelOffset;
         cellList[numCandCells].linkoptions = CELLTYPE_TX;
         numCandCells++;
         break; // only delete one cell
      }
   }
   
   if(numCandCells==0){
      return FALSE;
   }else{
      return TRUE;
   }
}

void sixtop_addCellsByState(
      uint8_t      slotframeID,
      uint8_t      numOfLinks,
      cellInfo_ht* cellList,
      open_addr_t* previousHop,
      uint8_t      state
   ){
   uint8_t     i;
   uint8_t     j;
   open_addr_t temp_neighbor;
  
   //set schedule according links
   
   j=0;
   for(i = 0;i<SCHEDULEIEMAXNUMCELLS;i++){
      //only schedule when the request side wants to schedule a tx cell
      if(cellList[i].linkoptions == CELLTYPE_TX){
         switch(state) {
            case SIX_WAIT_ADDRESPONSE_SENDDONE:
               memcpy(&temp_neighbor,previousHop,sizeof(open_addr_t));
               
               //add a RX link
               schedule_addActiveSlot(
                  cellList[i].tsNum,
                  CELLTYPE_RX,
                  FALSE,
                  cellList[i].choffset,
                  &temp_neighbor
               );
               
               break;
            case SIX_ADDRESPONSE_RECEIVED:
               memcpy(&temp_neighbor,previousHop,sizeof(open_addr_t));
               //add a TX link
               schedule_addActiveSlot(
                  cellList[i].tsNum,
                  CELLTYPE_TX,
                  FALSE,
                  cellList[i].choffset,
                  &temp_neighbor
               );
               break;
            default:
               //log error
               break;
         }
         j++;
         if(j==numOfLinks){
            break;
         }
      }
   }
}

void sixtop_removeCellsByState(
      uint8_t      slotframeID,
      uint8_t      numOfLink,
      cellInfo_ht* cellList,
      open_addr_t* previousHop
   ){
   uint8_t i;
   
   for(i=0;i<numOfLink;i++){   
      if(cellList[i].linkoptions == CELLTYPE_TX){
         schedule_removeActiveSlot(
            cellList[i].tsNum,
            previousHop
         );
      }
   }
}

bool sixtop_areAvailableCellsToBeScheduled(
      uint8_t      frameID, 
      uint8_t      numOfCells, 
      cellInfo_ht* cellList, 
      uint8_t      bandwidth
   ){
   uint8_t i;
   uint8_t bw;
   bool    available;
   
   i          = 0;
   bw         = bandwidth;
   available  = FALSE;
  
   if(bw == 0 || bw>SCHEDULEIEMAXNUMCELLS || numOfCells>SCHEDULEIEMAXNUMCELLS){
      // log wrong parameter error TODO
    
      available = FALSE;
   } else {
      do {
         if(schedule_isSlotOffsetAvailable(cellList[i].tsNum) == TRUE){
            bw--;
         } else {
            cellList[i].linkoptions = CELLTYPE_OFF;
         }
         i++;
      }while(i<numOfCells && bw>0);
      
      if(bw==0){
         //the rest link will not be scheduled, mark them as off type
         while(i<numOfCells){
            cellList[i].linkoptions = CELLTYPE_OFF;
            i++;
         }
         // local schedule can statisfy the bandwidth of cell request. 
         available = TRUE;
      } else {
         // local schedule can't statisfy the bandwidth of cell request
         available = FALSE;
      }
   }
   
   return available;
}
