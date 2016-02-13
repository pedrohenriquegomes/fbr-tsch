#include "opendefs.h"
#include "IEEE802154E.h"
#include "radio.h"
#include "radiotimer.h"
#include "IEEE802154.h"
#include "openqueue.h"
#include "idmanager.h"
#include "openserial.h"
#include "schedule.h"
#include "packetfunctions.h"
#include "openrandom.h"
#include "scheduler.h"
#include "leds.h"
#include "neighbors.h"
#include "debugpins.h"
#include "sixtop.h"
#include "adaptive_sync.h"
#include "processIE.h"
#include "light.h"
#include "sensors.h"
#include "topology.h"

//=========================== variables =======================================

ieee154e_vars_t    ieee154e_vars;
ieee154e_stats_t   ieee154e_stats;
ieee154e_dbg_t     ieee154e_dbg;

//=========================== prototypes ======================================

// SYNCHRONIZING
void     activity_synchronize_newSlot(void);
void     activity_synchronize_startOfFrame(PORT_RADIOTIMER_WIDTH capturedTime);
void     activity_synchronize_endOfFrame(PORT_RADIOTIMER_WIDTH capturedTime);
// TX
void     activity_ti1ORri1(void);
void     activity_ti2(void);
void     activity_tie1(void);
void     activity_ti3(void);
void     activity_tie2(void);
void     activity_ti4(PORT_RADIOTIMER_WIDTH capturedTime);
void     activity_tie3(void);
void     activity_ti5(PORT_RADIOTIMER_WIDTH capturedTime);

// RX
void     activity_ri2(void);
void     activity_rie1(void);
void     activity_ri3(void);
void     activity_rie2(void);
void     activity_ri4(PORT_RADIOTIMER_WIDTH capturedTime);
void     activity_rie3(void);
void     activity_ri5(PORT_RADIOTIMER_WIDTH capturedTime);

// frame validity check
bool     isValidRxFrame(ieee802154_header_iht* ieee802514_header);
// ASN handling
void     incrementAsnOffset(void);
void     ieee154e_syncSlotOffset(void);
void     asnStoreFromEB(uint8_t* asn);
void     joinPriorityStoreFromEB(uint8_t jp);

// timeslot template handling
void     timeslotTemplateIDStoreFromEB(uint8_t id);
// channelhopping template handling
void     channelhoppingTemplateIDStoreFromEB(uint8_t id);
// synchronization
void     synchronizePacket(PORT_RADIOTIMER_WIDTH timeReceived);
void     changeIsSync(bool newIsSync);
// notifying upper layer
void     notif_sendDone(OpenQueueEntry_t* packetSent, owerror_t error);
void     notif_receive(OpenQueueEntry_t* packetReceived);
// statistics
void     resetStats(void);
void     updateStats(PORT_SIGNED_INT_WIDTH timeCorrection);
// misc
uint8_t  calculateFrequency(uint8_t channelOffset);
void     changeState(ieee154e_state_t newstate);
void     endSlot(void);
bool     debugPrint_asn(void);
bool     debugPrint_isSync(void);
// interrupts
void     isr_ieee154e_newSlot(void);
void     isr_ieee154e_timer(void);

//=========================== admin ===========================================

/**
\brief This function initializes this module.

Call this function once before any other function in this module, possibly
during boot-up.
*/
void ieee154e_init() {
   
   // initialize variables
   memset(&ieee154e_vars,0,sizeof(ieee154e_vars_t));
   memset(&ieee154e_dbg,0,sizeof(ieee154e_dbg_t));
   
   ieee154e_vars.singleChannel     = 0;
   ieee154e_vars.nextChannelEB     = SYNCHRONIZING_CHANNEL - 11;
   
   ieee154e_vars.isAckEnabled      = TRUE;
   ieee154e_vars.isSecurityEnabled = FALSE;
   // default hopping template
   memcpy(
       &(ieee154e_vars.chTemplate[0]),
       chTemplate_default,
       sizeof(ieee154e_vars.chTemplate)
   );
   
   
  memcpy(
       &(ieee154e_vars.chTemplateEB[0]),
       chTemplate_eb,
       sizeof(ieee154e_vars.chTemplateEB)
   );
   
   if (idmanager_getIsDAGroot()==TRUE) {
      changeIsSync(TRUE);
   } else {
      changeIsSync(FALSE);
   }
   
   resetStats();
   ieee154e_stats.numDeSync                 = 0;
   
   // switch radio on
   radio_rfOn();
   
   // set callback functions for the radio
   radio_setOverflowCb(isr_ieee154e_newSlot);
   radio_setCompareCb(isr_ieee154e_timer);
   radio_setStartFrameCb(ieee154e_startOfFrame);
   radio_setEndFrameCb(ieee154e_endOfFrame);
   // have the radio start its timer
   ieee154e_vars.syncSlotLength = TsSlotDuration;
   radio_startTimer(TsSlotDuration);
}

//=========================== public ==========================================

/**
/brief Difference between some older ASN and the current ASN.

\param[in] someASN some ASN to compare to the current

\returns The ASN difference, or 0xffff if more than 65535 different
*/
PORT_RADIOTIMER_WIDTH ieee154e_asnDiff(asn_t* someASN) {
   PORT_RADIOTIMER_WIDTH diff;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   if (ieee154e_vars.asn.byte4 != someASN->byte4) {
      ENABLE_INTERRUPTS();
      return (PORT_RADIOTIMER_WIDTH)0xFFFFFFFF;;
   }
   
   diff = 0;
   if (ieee154e_vars.asn.bytes2and3 == someASN->bytes2and3) {
      ENABLE_INTERRUPTS();
      return ieee154e_vars.asn.bytes0and1-someASN->bytes0and1;
   } else if (ieee154e_vars.asn.bytes2and3-someASN->bytes2and3==1) {
      diff  = ieee154e_vars.asn.bytes0and1;
      diff += 0xffff-someASN->bytes0and1;
      diff += 1;
   } else {
      diff = (PORT_RADIOTIMER_WIDTH)0xFFFFFFFF;;
   }
   ENABLE_INTERRUPTS();
   return diff;
}

//======= events

/**
\brief Indicates a new slot has just started.

This function executes in ISR mode, when the new slot timer fires.
*/
void isr_ieee154e_newSlot() {
   if (ieee154e_vars.isSync==FALSE) {
      radio_setTimerPeriod(ieee154e_vars.syncSlotLength);
      ieee154e_vars.syncSlotLength = TsSlotDuration;
      debugpins_slot_set();
      debugpins_slot_clr();
      if (idmanager_getIsDAGroot()==TRUE) {
         changeIsSync(TRUE);
         incrementAsnOffset();
         ieee154e_syncSlotOffset();
         ieee154e_vars.nextActiveSlotOffset = schedule_getNextActiveSlotOffset();
      } else {
         activity_synchronize_newSlot();
      }
   } else {
      radio_setTimerPeriod(TsSlotDuration);
      activity_ti1ORri1();
   }
   ieee154e_dbg.num_newSlot++;
}

/**
\brief Indicates the FSM timer has fired.

This function executes in ISR mode, when the FSM timer fires.
*/
void isr_ieee154e_timer() {
   switch (ieee154e_vars.state) {
      case S_TXDATAOFFSET:
         activity_ti2();
         break;
      case S_TXDATAPREPARE:
         activity_tie1();
         break;
      case S_TXDATAREADY:
         activity_ti3();
         break;
      case S_TXDATADELAY:
         activity_tie2();
         break;
      case S_TXDATA:
         activity_tie3();
         break;
      case S_RXDATAOFFSET:
         activity_ri2(); 
         break;
      case S_RXDATAPREPARE:
         activity_rie1();
         break;
      case S_RXDATAREADY:
         activity_ri3();
         break;
      case S_RXDATALISTEN:
         activity_rie2();
         break;
      case S_RXDATA:
         activity_rie3();
         break;
      default:
         // log the error
         openserial_printError(COMPONENT_IEEE802154E,ERR_WRONG_STATE_IN_TIMERFIRES,
                               (errorparameter_t)ieee154e_vars.state,
                               (errorparameter_t)ieee154e_vars.slotOffset);
         // abort
         endSlot();
         break;
   }
   ieee154e_dbg.num_timer++;
}

/**
\brief Indicates the radio just received the first byte of a packet.

This function executes in ISR mode.
*/
void ieee154e_startOfFrame(PORT_RADIOTIMER_WIDTH capturedTime) {
   if (ieee154e_vars.isSync==FALSE) {
     activity_synchronize_startOfFrame(capturedTime);
   } else {
      switch (ieee154e_vars.state) {
         case S_TXDATADELAY:   
            activity_ti4(capturedTime);
            break;
         case S_RXDATAREADY:
            /*
            Similarly as above.
            */
            // no break!
         case S_RXDATALISTEN:
            activity_ri4(capturedTime);
            break;
         default:
            // log the error
            openserial_printError(COMPONENT_IEEE802154E,ERR_WRONG_STATE_IN_NEWSLOT,
                                  (errorparameter_t)ieee154e_vars.state,
                                  (errorparameter_t)ieee154e_vars.slotOffset);
            // abort
            endSlot();
            break;
      }
   }
   ieee154e_dbg.num_startOfFrame++;
}

/**
\brief Indicates the radio just received the last byte of a packet.

This function executes in ISR mode.
*/
void ieee154e_endOfFrame(PORT_RADIOTIMER_WIDTH capturedTime) {
   if (ieee154e_vars.isSync==FALSE) {
      activity_synchronize_endOfFrame(capturedTime);
   } else {
      switch (ieee154e_vars.state) {
         case S_TXDATA:
            activity_ti5(capturedTime);
            break;
         case S_RXDATA:
            activity_ri5(capturedTime);
            break;
         default:
            // log the error
            openserial_printError(COMPONENT_IEEE802154E,ERR_WRONG_STATE_IN_ENDOFFRAME,
                                  (errorparameter_t)ieee154e_vars.state,
                                  (errorparameter_t)ieee154e_vars.slotOffset);
            // abort
            endSlot();
            break;
      }
   }
   ieee154e_dbg.num_endOfFrame++;
}

//======= misc

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_asn() {
   asn_t output;
   output.byte4         =  ieee154e_vars.asn.byte4;
   output.bytes2and3    =  ieee154e_vars.asn.bytes2and3;
   output.bytes0and1    =  ieee154e_vars.asn.bytes0and1;
   openserial_printStatus(STATUS_ASN,(uint8_t*)&output,sizeof(output));
   return TRUE;
}

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_isSync() {
   uint8_t output=0;
   output = ieee154e_vars.isSync;
   openserial_printStatus(STATUS_ISSYNC,(uint8_t*)&output,sizeof(uint8_t));
   return TRUE;
}

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_macStats() {
   // send current stats over serial
   openserial_printStatus(STATUS_MACSTATS,(uint8_t*)&ieee154e_stats,sizeof(ieee154e_stats_t));
   return TRUE;
}

//=========================== private =========================================

//======= SYNCHRONIZING

port_INLINE void activity_synchronize_newSlot() {
   uint8_t i;
   ieee154e_vars.joinChannelChangingCounter = (ieee154e_vars.joinChannelChangingCounter + 1)%EB_SLOWHOPPING_PERIOD;
  
   // I'm in the middle of receiving a packet
   if (ieee154e_vars.state==S_SYNCRX) {
      return;
   }
   
   // if this is the first time I call this function while not synchronized,
   // switch on the radio in Rx mode
   if (ieee154e_vars.state!=S_SYNCLISTEN) {
      // change state
      changeState(S_SYNCLISTEN);
      
      // turn off the radio (in case it wasn't yet)
      radio_rfOff();
      
      // configure the radio to listen to the default synchronizing channel
      radio_setFrequency(SYNCHRONIZING_CHANNEL);
      
      // update record of current channel
      ieee154e_vars.freq = SYNCHRONIZING_CHANNEL;
      
      // switch on the radio in Rx mode.
      radio_rxEnable();
      ieee154e_vars.radioOnInit=radio_getTimerValue();
      ieee154e_vars.radioOnThisSlot=TRUE;
      radio_rxNow();
   }
   
   if (ieee154e_vars.state==S_SYNCLISTEN && ieee154e_vars.joinChannelChangingCounter == 0){
      // turn off the radio (in case it wasn't yet)
      radio_rfOff();
      
      i=0;
      while (ieee154e_vars.freq-11 != ieee154e_vars.chTemplateEB[i] && i<EB_NUMCHANS){
          i++;
      }
      
      if (i<EB_NUMCHANS){
          ieee154e_vars.freq = 11 + ieee154e_vars.chTemplateEB[(i+1)%EB_NUMCHANS];
      }
      
      // configure the radio to listen to the default synchronizing channel
      radio_setFrequency(ieee154e_vars.freq);
      
      // switch on the radio in Rx mode.
      radio_rxEnable();
      ieee154e_vars.radioOnInit=radio_getTimerValue();
      ieee154e_vars.radioOnThisSlot=TRUE;
      radio_rxNow();
   }
   
   // if I'm already in S_SYNCLISTEN, while not synchronized,
   // but the synchronizing channel has been changed,
   // change the synchronizing channel
   if ((ieee154e_vars.state==S_SYNCLISTEN) && (ieee154e_vars.singleChannelChanged == TRUE)) {
      // turn off the radio (in case it wasn't yet)
      radio_rfOff();
      
      // update record of current channel
      ieee154e_vars.freq = calculateFrequency(ieee154e_vars.singleChannel);
      
      // configure the radio to listen to the default synchronizing channel
      radio_setFrequency(ieee154e_vars.freq);
      
      // switch on the radio in Rx mode.
      radio_rxEnable();
      ieee154e_vars.radioOnInit=radio_getTimerValue();
      ieee154e_vars.radioOnThisSlot=TRUE;
      radio_rxNow();
      ieee154e_vars.singleChannelChanged = FALSE;
   }
   
   // increment ASN (used only to schedule serial activity)
   incrementAsnOffset();
   
   // to be able to receive and transmist serial even when not synchronized
   // take turns every 8 slots sending and receiving
   if        ((ieee154e_vars.asn.bytes0and1&0x000f)==0x0000) {
      openserial_stop();
      openserial_startOutput();
   } else if ((ieee154e_vars.asn.bytes0and1&0x000f)==0x0008) {
      openserial_stop();
      openserial_startInput();
   }
}

port_INLINE void activity_synchronize_startOfFrame(PORT_RADIOTIMER_WIDTH capturedTime) {
   
   // don't care about packet if I'm not listening
   if (ieee154e_vars.state!=S_SYNCLISTEN) {
      return;
   }
   
   // change state
   changeState(S_SYNCRX);
   
   // stop the serial
   openserial_stop();
   
   // record the captured time 
   ieee154e_vars.lastCapturedTime = capturedTime;
   
   // record the captured time (for sync)
   ieee154e_vars.syncCapturedTime = capturedTime;
}

port_INLINE void activity_synchronize_endOfFrame(PORT_RADIOTIMER_WIDTH capturedTime) {
   uint8_t    i;
   eb_ht*     eb;
   
   // check state
   if (ieee154e_vars.state!=S_SYNCRX) {
      // log the error
      openserial_printError(COMPONENT_IEEE802154E,ERR_WRONG_STATE_IN_ENDFRAME_SYNC,
                            (errorparameter_t)ieee154e_vars.state,
                            (errorparameter_t)0);
      // abort
      endSlot();
   }
   
   // change state
   changeState(S_SYNCPROC);
   
   // get a buffer to put the (received) frame in
   ieee154e_vars.dataReceived = openqueue_getFreePacketBuffer(COMPONENT_IEEE802154E);
   if (ieee154e_vars.dataReceived==NULL) {
      // log the error
      openserial_printError(COMPONENT_IEEE802154E,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      // abort
      endSlot();
      return;
   }
   
   // declare ownership over that packet
   ieee154e_vars.dataReceived->creator = COMPONENT_IEEE802154E;
   ieee154e_vars.dataReceived->owner   = COMPONENT_IEEE802154E;
   
   /*
   The do-while loop that follows is a little parsing trick.
   Because it contains a while(0) condition, it gets executed only once.
   The behavior is:
   - if a break occurs inside the do{} body, the error code below the loop
     gets executed. This indicates something is wrong with the packet being 
     parsed.
   - if a return occurs inside the do{} body, the error code below the loop
     does not get executed. This indicates the received packet is correct.
   */
   do { // this "loop" is only executed once
      
      // retrieve the received data frame from the radio's Rx buffer
      ieee154e_vars.dataReceived->payload = &(ieee154e_vars.dataReceived->packet[FIRST_FRAME_BYTE]);
      radio_getReceivedFrame(       ieee154e_vars.dataReceived->payload,
                                   &ieee154e_vars.dataReceived->length,
                             sizeof(ieee154e_vars.dataReceived->packet),
                                   &ieee154e_vars.dataReceived->l1_rssi,
                                   &ieee154e_vars.dataReceived->l1_lqi,
                                   &ieee154e_vars.dataReceived->l1_crc);
      
      // break if wrong length
      if (ieee154e_vars.dataReceived->length!=sizeof(eb_ht)+2) {
         break;
      }
      
      // break if invalid CRC
      if (ieee154e_vars.dataReceived->l1_crc==FALSE) {
         break;
      }
      
      // break if not beacon
      if (((eb_ht*)(ieee154e_vars.dataReceived->payload))->type != LONGTYPE_BEACON) {
         break;
      }
      
      // break if from node outside of allowed topology
      if (topology_isAcceptablePacket(((eb_ht*)(ieee154e_vars.dataReceived->payload))->src)==FALSE) {
         break;
      }
      
      // break if I received packet less than RESYNCHRONIZATIONGUARD from slot edge
      // (we will wait for next EB)
      if (
            TsSlotDuration-ieee154e_vars.syncCapturedTime<RESYNCHRONIZATIONGUARD
      ) {
         ieee154e_vars.syncSlotLength = (TsSlotDuration*25)/10;
         break;
      }
      
      //=== if I get here, I got a valid beacon at the right time, I can stop listening
      
      // turn off the radio
      radio_rfOff();
      
      //=== synchronize to the ASN
      // store ASN
      // store the ASN
      eb = (eb_ht*)(ieee154e_vars.dataReceived->payload);
      ieee154e_vars.asn.bytes0and1   =     eb->asn0+
                                       256*eb->asn1;
      ieee154e_vars.asn.bytes2and3   =     eb->asn2+
                                       256*eb->asn3;
      ieee154e_vars.asn.byte4        =     0;
      // calculate the current slotoffset
      ieee154e_syncSlotOffset();
      schedule_syncSlotOffset(ieee154e_vars.slotOffset);
      ieee154e_vars.nextActiveSlotOffset = schedule_getNextActiveSlotOffset();
      // infer the asnOffset based on the fact that
      // ieee154e_vars.freq = 11 + (asnOffset + channelOffset)%16 
      for (i=0;i<EB_NUMCHANS;i++){
         if ((ieee154e_vars.freq - 11)==ieee154e_vars.chTemplateEB[i]){
            break;
         }
      }
      ieee154e_vars.ebAsnOffset = i - schedule_getChannelOffset();      
      ieee154e_vars.dataAsnOffset = ieee154e_vars.asn.bytes0and1%16 - schedule_getChannelOffset();
      
      // compute radio duty cycle
      ieee154e_vars.radioOnTics += (radio_getTimerValue()-ieee154e_vars.radioOnInit);
      
      // toss CRC (2 last bytes)
      packetfunctions_tossFooter(ieee154e_vars.dataReceived, LENGTH_CRC);
      
      // synchronize to the slot boundary
      synchronizePacket(ieee154e_vars.syncCapturedTime);
      
      // declare synchronized
      changeIsSync(TRUE);
      
      // log the info
      openserial_printInfo(
         COMPONENT_IEEE802154E,
         ERR_SYNCHRONIZED,
         (errorparameter_t)ieee154e_vars.slotOffset,
         (errorparameter_t)0
      );
      
      // send received EB up the stack to 6top
      notif_receive(ieee154e_vars.dataReceived);
      
      // clear local variable
      ieee154e_vars.dataReceived = NULL;
      
      // official end of synchronization
      endSlot();
      
      // everything went well, return here not to execute the error code below
      return;
      
   } while(0);
   
   // free the (invalid) received data buffer so RAM memory can be recycled
   openqueue_freePacketBuffer(ieee154e_vars.dataReceived);
   
   // clear local variable
   ieee154e_vars.dataReceived = NULL;
   
   // return to listening state
   changeState(S_SYNCLISTEN);
}

//======= TX

port_INLINE void activity_ti1ORri1() {
   cellType_t  cellType;
   eb_ht*      eb;
   
   // increment ASN (do this first so debug pins are in sync)
   incrementAsnOffset();
   
   // wiggle debug pins
   debugpins_slot_toggle();
   if (ieee154e_vars.slotOffset==0) {
      debugpins_frame_toggle();
   }
   
   // desynchronize if needed
   if (idmanager_getIsDAGroot()==FALSE) {
      ieee154e_vars.deSyncTimeout--;
      if (ieee154e_vars.deSyncTimeout==0) {
         // declare myself desynchronized
         changeIsSync(FALSE);
        
         // log the error
         openserial_printError(COMPONENT_IEEE802154E,ERR_DESYNCHRONIZED,
                               (errorparameter_t)ieee154e_vars.slotOffset,
                               (errorparameter_t)0);
            
         // update the statistics
         ieee154e_stats.numDeSync++;
         
         // abort
         endSlot();
         return;
      }
   }
   
   // if the previous slot took too long, we will not be in the right state
   if (ieee154e_vars.state!=S_SLEEP) {
      // log the error
      openserial_printError(COMPONENT_IEEE802154E,ERR_WRONG_STATE_IN_STARTSLOT,
                            (errorparameter_t)ieee154e_vars.state,
                            (errorparameter_t)ieee154e_vars.slotOffset);
      // abort
      endSlot();
      return;
   }
   
    // lets check for missed toogles
   light_check_missed_toogles();
   
   // trigger application, which can send packet
   light_trigger();
   
   if (ieee154e_vars.slotOffset==ieee154e_vars.nextActiveSlotOffset) {
      // this is the next active slot
      
      // advance the schedule
      schedule_advanceSlot();
      
      // find the next one
      ieee154e_vars.nextActiveSlotOffset = schedule_getNextActiveSlotOffset();
   } else {
      // this is NOT the next active slot, abort
      // stop using serial
      openserial_stop();
      // abort the slot
      endSlot();
      //start outputing serial
      openserial_startOutput();
      return;
   }
   
   // check the schedule to see what type of slot this is
   cellType = schedule_getType();
   switch (cellType) {
      case CELLTYPE_EB:
         // have 6top create an EB packet every AVERAGEDEGREE EB slots, on average
         if (openrandom_get16b()%AVERAGEDEGREE==0) {
            sixtop_sendEB();
         }
      case CELLTYPE_TXRX:
         // stop using serial
         openserial_stop();
         // assuming that there is nothing to send
         ieee154e_vars.dataToSend = NULL;
         // check whether there is something to send
         if (cellType==CELLTYPE_EB) {
            // CELLTYPE_EB
            ieee154e_vars.dataToSend = openqueue_macGetEBPacket();
         } else {
            // CELLTYPE_TXRX
            if (openrandom_get16b()%AVERAGEDEGREE==0) {
               ieee154e_vars.dataToSend = openqueue_macGetDataPacket();
            }
         }
         
         if (ieee154e_vars.dataToSend==NULL) {
            // listen
            
            // change state
            changeState(S_RXDATAOFFSET);
            // arm rt1
            radiotimer_schedule(DURATION_rt1);
         } else {
            // transmit
            
            // change state
            changeState(S_TXDATAOFFSET);
            // change owner
            ieee154e_vars.dataToSend->owner = COMPONENT_IEEE802154E;
            if (cellType==CELLTYPE_EB) {
               // I will be sending an EB
               
               // fill in the ASN field of the EB
              eb = (eb_ht*)(ieee154e_vars.dataToSend->payload);
              eb->asn0 = (ieee154e_vars.asn.bytes0and1     & 0xff);
              eb->asn1 = (ieee154e_vars.asn.bytes0and1/256 & 0xff);
              eb->asn2 = (ieee154e_vars.asn.bytes2and3     & 0xff);
              eb->asn3 = (ieee154e_vars.asn.bytes2and3/256 & 0xff);
            }
            // record that I attempt to transmit this packet
            ieee154e_vars.dataToSend->l2_numTxAttempts++;
            // arm tt1
            radiotimer_schedule(DURATION_tt1);
         }
         break;
      default:
         // stop using serial
         openserial_stop();
         // log the error
         openserial_printCritical(COMPONENT_IEEE802154E,ERR_WRONG_CELLTYPE,
                               (errorparameter_t)cellType,
                               (errorparameter_t)ieee154e_vars.slotOffset);
         // abort
         endSlot();
         break;
   }
}

port_INLINE void activity_ti2() {
   
   // change state
   changeState(S_TXDATAPREPARE);

   // make a local copy of the frame
   packetfunctions_duplicatePacket(&ieee154e_vars.localCopyForTransmission, ieee154e_vars.dataToSend);
   
   // add 2 CRC bytes only to the local copy as we end up here for each retransmission
   packetfunctions_reserveFooterSize(&ieee154e_vars.localCopyForTransmission, 2);
   
   // calculate the frequency to transmit on
   ieee154e_vars.freq = calculateFrequency(schedule_getChannelOffset()); 
   
   // configure the radio for that frequency
   radio_setFrequency(ieee154e_vars.freq);
   
   // load the packet in the radio's Tx buffer
   radio_loadPacket(ieee154e_vars.localCopyForTransmission.payload,
                    ieee154e_vars.localCopyForTransmission.length);
   
   // enable the radio in Tx mode. This does not send the packet.
   radio_txEnable();
   
   ieee154e_vars.radioOnInit=radio_getTimerValue();
   ieee154e_vars.radioOnThisSlot=TRUE;
   // arm tt2
   radiotimer_schedule(DURATION_tt2);
   
   // change state
   changeState(S_TXDATAREADY);
}

port_INLINE void activity_tie1() {
   // log the error
   openserial_printError(COMPONENT_IEEE802154E,ERR_MAXTXDATAPREPARE_OVERFLOW,
                         (errorparameter_t)ieee154e_vars.state,
                         (errorparameter_t)ieee154e_vars.slotOffset);
   
   // abort
   endSlot();
}

port_INLINE void activity_ti3() {
   // change state
   changeState(S_TXDATADELAY);
   
   // arm tt3
   radiotimer_schedule(DURATION_tt3);
   
   // give the 'go' to transmit
   radio_txNow();
}

port_INLINE void activity_tie2() {
   // log the error
   openserial_printError(COMPONENT_IEEE802154E,ERR_WDRADIO_OVERFLOWS,
                         (errorparameter_t)ieee154e_vars.state,
                         (errorparameter_t)ieee154e_vars.slotOffset);
   
   // abort
   endSlot();
}

//start of frame interrupt
port_INLINE void activity_ti4(PORT_RADIOTIMER_WIDTH capturedTime) {
   // change state
   changeState(S_TXDATA);
   
   // cancel tt3
   radiotimer_cancel();
   
   // record the captured time
   ieee154e_vars.lastCapturedTime = capturedTime;
   
   // arm tt4
   radiotimer_schedule(DURATION_tt4);
}

port_INLINE void activity_tie3() {
   // log the error
   openserial_printError(COMPONENT_IEEE802154E,ERR_WDDATADURATION_OVERFLOWS,
                         (errorparameter_t)ieee154e_vars.state,
                         (errorparameter_t)ieee154e_vars.slotOffset);
   
   // abort
   endSlot();
}

port_INLINE void activity_ti5(PORT_RADIOTIMER_WIDTH capturedTime) {
   
   // change state
   changeState(S_RXACKOFFSET);
   
   // cancel tt4
   radiotimer_cancel();
   
   // turn off the radio
   radio_rfOff();
   
   ieee154e_vars.radioOnTics+=(radio_getTimerValue()-ieee154e_vars.radioOnInit);
   
   // record the captured time
   ieee154e_vars.lastCapturedTime = capturedTime;
   
   // we do not listen for ACK
      
    // indicate succesful Tx to schedule to keep statistics
    schedule_indicateTx(&ieee154e_vars.asn,TRUE);
    // indicate to upper later the packet was sent successfully
    notif_sendDone(ieee154e_vars.dataToSend,E_SUCCESS);
    // reset local variable
    ieee154e_vars.dataToSend = NULL;
    // abort
    endSlot();
}

//======= RX

port_INLINE void activity_ri2() {
   // change state
   changeState(S_RXDATAPREPARE);
   
   // calculate the frequency to transmit on
   ieee154e_vars.freq = calculateFrequency(schedule_getChannelOffset()); 
   
   // configure the radio for that frequency
   radio_setFrequency(ieee154e_vars.freq);
   
   // enable the radio in Rx mode. The radio does not actively listen yet.
   radio_rxEnable();
   
   ieee154e_vars.radioOnInit=radio_getTimerValue();
   ieee154e_vars.radioOnThisSlot=TRUE;
   
   // arm rt2
   radiotimer_schedule(DURATION_rt2);
       
   // change state
   changeState(S_RXDATAREADY);
}

port_INLINE void activity_rie1() {
   // log the error
   openserial_printError(COMPONENT_IEEE802154E,ERR_MAXRXDATAPREPARE_OVERFLOWS,
                         (errorparameter_t)ieee154e_vars.state,
                         (errorparameter_t)ieee154e_vars.slotOffset);
   
   // abort
   endSlot();
}

port_INLINE void activity_ri3() {
   // change state
   changeState(S_RXDATALISTEN);
   
   // give the 'go' to receive
   radio_rxNow();
   
   // arm rt3 
   radiotimer_schedule(DURATION_rt3);
}

port_INLINE void activity_rie2() {
   // abort
   endSlot();
}

port_INLINE void activity_ri4(PORT_RADIOTIMER_WIDTH capturedTime) {

   // change state
   changeState(S_RXDATA);
   
   // cancel rt3
   radiotimer_cancel();
   
   // record the captured time
   ieee154e_vars.lastCapturedTime = capturedTime;
   
   // record the captured time to sync
   ieee154e_vars.syncCapturedTime = capturedTime;

   radiotimer_schedule(DURATION_rt4);
}

port_INLINE void activity_rie3() {
     
   // log the error
   openserial_printError(COMPONENT_IEEE802154E,ERR_WDDATADURATION_OVERFLOWS,
                         (errorparameter_t)ieee154e_vars.state,
                         (errorparameter_t)ieee154e_vars.slotOffset);
   
   // abort
   endSlot();
}

port_INLINE void activity_ri5(PORT_RADIOTIMER_WIDTH capturedTime) {
   eb_ht* eb;
   
   // change state
   changeState(S_TXACKOFFSET);
   
   // cancel rt4
   radiotimer_cancel();
   
   // turn off the radio
   radio_rfOff();
   
   // adjust power calculation
   ieee154e_vars.radioOnTics+=radio_getTimerValue()-ieee154e_vars.radioOnInit;
   
   // get a buffer to put the (received) data in
   ieee154e_vars.dataReceived = openqueue_getFreePacketBuffer(COMPONENT_IEEE802154E);
   if (ieee154e_vars.dataReceived==NULL) {
      // log the error
      openserial_printError(COMPONENT_IEEE802154E,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      // abort
      endSlot();
      return;
   }
   
   // declare ownership over that packet
   ieee154e_vars.dataReceived->creator = COMPONENT_IEEE802154E;
   ieee154e_vars.dataReceived->owner   = COMPONENT_IEEE802154E;

   /*
   The do-while loop that follows is a little parsing trick.
   Because it contains a while(0) condition, it gets executed only once.
   The behavior is:
   - if a break occurs inside the do{} body, the error code below the loop
     gets executed. This indicates something is wrong with the packet being 
     parsed.
   - if a return occurs inside the do{} body, the error code below the loop
     does not get executed. This indicates the received packet is correct.
   */
   do { // this "loop" is only executed once
      
      // retrieve the received data frame from the radio's Rx buffer
      ieee154e_vars.dataReceived->payload = &(ieee154e_vars.dataReceived->packet[FIRST_FRAME_BYTE]);
      radio_getReceivedFrame(       ieee154e_vars.dataReceived->payload,
                                   &ieee154e_vars.dataReceived->length,
                             sizeof(ieee154e_vars.dataReceived->packet),
                                   &ieee154e_vars.dataReceived->l1_rssi,
                                   &ieee154e_vars.dataReceived->l1_lqi,
                                   &ieee154e_vars.dataReceived->l1_crc);
      
      // break if wrong length
      if (ieee154e_vars.dataReceived->length<LENGTH_CRC || ieee154e_vars.dataReceived->length>LENGTH_IEEE154_MAX ) {
         // jump to the error code below this do-while loop
        openserial_printError(COMPONENT_IEEE802154E,ERR_INVALIDPACKETFROMRADIO,
                            (errorparameter_t)2,
                            ieee154e_vars.dataReceived->length);
         break;
      }
      
      // toss CRC (2 last bytes)
      packetfunctions_tossFooter(   ieee154e_vars.dataReceived, LENGTH_CRC);
      
      // break if invalid CRC
      if (ieee154e_vars.dataReceived->l1_crc==FALSE) {
         // jump to the error code below this do-while loop
         break;
      }
      
      // parse as if it's an EB (light_ht and eb_ht) start with the same bytes
      eb = (eb_ht*)ieee154e_vars.dataReceived->payload;
      
      // break if wrong type
      if ( eb->type!=LONGTYPE_BEACON && eb->type!=LONGTYPE_DATA) {
         break;
      }
      
      // break if from node outside of allowable topology
      if (topology_isAcceptablePacket(eb->src)==FALSE) {
         break;
      }
      
      // record the captured time
      ieee154e_vars.lastCapturedTime = capturedTime;
      
      // synchronize to the received packet iif I'm not a DAGroot and this is my preferred parent
      if (
         idmanager_getIsDAGroot()==FALSE &&
         neighbors_isPreferredParent(eb->src)
      ) {
         synchronizePacket(ieee154e_vars.syncCapturedTime);
      }
      
      // indicate reception to upper layer
      notif_receive(ieee154e_vars.dataReceived);
      
      // reset local variable
      ieee154e_vars.dataReceived = NULL;
      
      // abort
      endSlot();
      
      // everything went well, return here not to execute the error code below
      return;
      
   } while(0);
   
   // free the (invalid) received data so RAM memory can be recycled
   openqueue_freePacketBuffer(ieee154e_vars.dataReceived);
   
   // clear local variable
   ieee154e_vars.dataReceived = NULL;
   
   // abort
   endSlot();
}

//======= frame validity check

/**
\brief Decides whether the packet I just received is valid received frame.

A valid Rx frame satisfies the following constraints:
- its IEEE802.15.4 header is well formatted
- it's a DATA of BEACON frame (i.e. not ACK and not COMMAND)
- it's sent on the same PANid as mine
- it's for me (unicast or broadcast)

\param[in] ieee802514_header IEEE802.15.4 header of the packet I just received

\returns TRUE if packet is valid received frame, FALSE otherwise
*/
port_INLINE bool isValidRxFrame(ieee802154_header_iht* ieee802514_header) {
   return ieee802514_header->valid==TRUE                                                           && \
          (
             ieee802514_header->frameType==IEEE154_TYPE_DATA                   ||
             ieee802514_header->frameType==IEEE154_TYPE_BEACON
          )                                                                                        && \
          packetfunctions_sameAddress(&ieee802514_header->panid,idmanager_getMyID(ADDR_PANID))     && \
          (
             idmanager_isMyAddress(&ieee802514_header->dest)                   ||
             packetfunctions_isBroadcastMulticast(&ieee802514_header->dest)
          );
}

//======= ASN handling

port_INLINE void incrementAsnOffset() {
   
   // increment the asn
   ieee154e_vars.asn.bytes0and1++;
   if (ieee154e_vars.asn.bytes0and1==0) {
      ieee154e_vars.asn.bytes2and3++;
      if (ieee154e_vars.asn.bytes2and3==0) {
         ieee154e_vars.asn.byte4++;
      }
   }
   
   // increment the offsets
   ieee154e_vars.slotOffset  = (ieee154e_vars.slotOffset+1)%SLOTFRAME_LENGTH;
   ieee154e_vars.ebAsnOffset   = (ieee154e_vars.ebAsnOffset+1)%EB_NUMCHANS;
   ieee154e_vars.dataAsnOffset = (ieee154e_vars.dataAsnOffset+1)%16;
}

//from upper layer that want to send the ASN to compute timing or latency
port_INLINE void ieee154e_getAsn(uint8_t* array) {
   array[0]         = (ieee154e_vars.asn.bytes0and1     & 0xff);
   array[1]         = (ieee154e_vars.asn.bytes0and1/256 & 0xff);
   array[2]         = (ieee154e_vars.asn.bytes2and3     & 0xff);
   array[3]         = (ieee154e_vars.asn.bytes2and3/256 & 0xff);
   array[4]         =  ieee154e_vars.asn.byte4;
}

port_INLINE void ieee154e_getAsnStruct(asn_t* toAsn) {
   memcpy(toAsn,&ieee154e_vars.asn,sizeof(asn_t));
}

port_INLINE uint16_t ieee154e_getTimeCorrection() {
    int16_t returnVal;
    
    returnVal = (uint16_t)(ieee154e_vars.timeCorrection);
    
    return returnVal;
}

port_INLINE void joinPriorityStoreFromEB(uint8_t jp){
  ieee154e_vars.dataReceived->l2_joinPriority = jp;
  ieee154e_vars.dataReceived->l2_joinPriorityPresent = TRUE;     
}

port_INLINE void asnStoreFromEB(uint8_t* asn) {
   
   // store the ASN
   ieee154e_vars.asn.bytes0and1   =     asn[0]+
                                    256*asn[1];
   ieee154e_vars.asn.bytes2and3   =     asn[2]+
                                    256*asn[3];
   ieee154e_vars.asn.byte4        =     asn[4];
}

port_INLINE void ieee154e_syncSlotOffset() {
   uint32_t slotOffset;
   
   // determine the current slotOffset
   slotOffset = ieee154e_vars.asn.byte4;
   slotOffset = slotOffset % SLOTFRAME_LENGTH;
   slotOffset = slotOffset << 16;
   slotOffset = slotOffset + ieee154e_vars.asn.bytes2and3;
   slotOffset = slotOffset % SLOTFRAME_LENGTH;
   slotOffset = slotOffset << 16;
   slotOffset = slotOffset + ieee154e_vars.asn.bytes0and1;
   slotOffset = slotOffset % SLOTFRAME_LENGTH;
   
   ieee154e_vars.slotOffset       = (slotOffset_t) slotOffset;
}

void ieee154e_setIsAckEnabled(bool isEnabled){
    ieee154e_vars.isAckEnabled = isEnabled;
}

void ieee154e_setSingleChannel(uint8_t channel){
    if (
        (channel < 11 || channel > 26) &&
         channel != 0   // channel == 0 means channel hopping is enabled
    ) {
        // log wrong channel, should be  : (0, or 11~26)
        return;
    }
    ieee154e_vars.singleChannel = channel;
    ieee154e_vars.singleChannelChanged = TRUE;
}

// timeslot template handling
port_INLINE void timeslotTemplateIDStoreFromEB(uint8_t id){
    ieee154e_vars.tsTemplateId = id;
}

// channelhopping template handling
port_INLINE void channelhoppingTemplateIDStoreFromEB(uint8_t id){
    ieee154e_vars.chTemplateId = id;
}
//======= synchronization

void synchronizePacket(PORT_RADIOTIMER_WIDTH timeReceived) {
   PORT_SIGNED_INT_WIDTH timeCorrection;
   PORT_RADIOTIMER_WIDTH newPeriod;
   
   // calculate new period
   timeCorrection                 =  (PORT_SIGNED_INT_WIDTH)((PORT_SIGNED_INT_WIDTH)timeReceived-(PORT_SIGNED_INT_WIDTH)TsTxOffset);
   newPeriod                      =  TsSlotDuration;
   newPeriod                      =  (PORT_RADIOTIMER_WIDTH)((PORT_SIGNED_INT_WIDTH)newPeriod+timeCorrection);
   
   // resynchronize by applying the new period
   radio_setTimerPeriod(newPeriod);
   
   // reset the de-synchronization timeout
   ieee154e_vars.deSyncTimeout    = DESYNCTIMEOUT;
   
   // log a large timeCorrection
   if (
         ieee154e_vars.isSync==TRUE &&
         (
            timeCorrection<-LIMITLARGETIMECORRECTION ||
            timeCorrection> LIMITLARGETIMECORRECTION
         )
      ) {
      openserial_printError(COMPONENT_IEEE802154E,ERR_LARGE_TIMECORRECTION,
                            (errorparameter_t)timeCorrection,
                            (errorparameter_t)0);
   }
   
   // update the stats
   ieee154e_stats.numSyncPkt++;
   updateStats(timeCorrection);
}

void changeIsSync(bool newIsSync) {
   ieee154e_vars.isSync = newIsSync;
   
   if (ieee154e_vars.isSync==TRUE) {
      leds_sync_off();
      resetStats();
   } else {
      leds_sync_on();
   }
}

//======= notifying upper layer

void notif_sendDone(OpenQueueEntry_t* packetSent, owerror_t error) {
   // record the outcome of the trasmission attempt
   packetSent->l2_sendDoneError   = error;
   // record the current ASN
   memcpy(&packetSent->l2_asn,&ieee154e_vars.asn,sizeof(asn_t));
   // associate this packet with the virtual component
   // COMPONENT_IEEE802154E_TO_RES so RES can knows it's for it
   packetSent->owner              = COMPONENT_IEEE802154E_TO_SIXTOP;
   // post RES's sendDone task
   scheduler_push_task(task_sixtopNotifSendDone,TASKPRIO_SIXTOP_NOTIF_TXDONE);
   // wake up the scheduler
   SCHEDULER_WAKEUP();
}

void notif_receive(OpenQueueEntry_t* packetReceived) {
   // record the current ASN
   memcpy(&packetReceived->l2_asn, &ieee154e_vars.asn, sizeof(asn_t));
   
   // indicate reception to the schedule, to keep statistics
   schedule_indicateRx(&packetReceived->l2_asn);
   
   // associate this packet with the virtual component
   // COMPONENT_IEEE802154E_TO_SIXTOP so sixtop can knows it's for it
   packetReceived->owner          = COMPONENT_IEEE802154E_TO_SIXTOP;
   // post 6top's Receive task
   scheduler_push_task(task_sixtopNotifReceive,TASKPRIO_SIXTOP_NOTIF_RX);
   // wake up the scheduler
   SCHEDULER_WAKEUP();
}

//======= stats

port_INLINE void resetStats() {
   ieee154e_stats.numSyncPkt      =    0;
   ieee154e_stats.numSyncAck      =    0;
   ieee154e_stats.minCorrection   =  127;
   ieee154e_stats.maxCorrection   = -127;
   ieee154e_stats.numTicsOn       =    0;
   ieee154e_stats.numTicsTotal    =    0;
   // do not reset the number of de-synchronizations
}

void updateStats(PORT_SIGNED_INT_WIDTH timeCorrection) {
   // update minCorrection
   if (timeCorrection<ieee154e_stats.minCorrection) {
     ieee154e_stats.minCorrection = timeCorrection;
   }
   // update maxConnection
   if(timeCorrection>ieee154e_stats.maxCorrection) {
     ieee154e_stats.maxCorrection = timeCorrection;
   }
}

//======= misc

/**
\brief Calculates the frequency channel to transmit on, based on the 
absolute slot number and the channel offset of the requested slot.

During normal operation, the frequency used is a function of the 
channelOffset indicating in the schedule, and of the ASN of the
slot. This ensures channel hopping, consecutive packets sent in the same slot
in the schedule are done on a difference frequency channel.

During development, you can force single channel operation by having this
function return a constant channel number (between 11 and 26). This allows you
to use a single-channel sniffer; but you can not schedule two links on two
different channel offsets in the same slot.

\param[in] channelOffset channel offset for the current slot

\returns The calculated frequency channel, an integer between 11 and 26.
*/
port_INLINE uint8_t calculateFrequency(uint8_t channelOffset) {
    uint8_t cellType;
    cellType = schedule_getType();
    if (cellType!=CELLTYPE_EB){
        if (ieee154e_vars.singleChannel >= 11 && ieee154e_vars.singleChannel <= 26 ) {
            return ieee154e_vars.singleChannel; // single channel
        } else {
            // channel hopping enabled, use the channel depending on hopping template
            return 11 + ieee154e_vars.chTemplate[(ieee154e_vars.dataAsnOffset+channelOffset)%16];
        }
    } else {
        return 11+ieee154e_vars.chTemplateEB[(ieee154e_vars.ebAsnOffset+channelOffset)%EB_NUMCHANS];
    }
}

/**
\brief Changes the state of the IEEE802.15.4e FSM.

Besides simply updating the state global variable,
this function toggles the FSM debug pin.

\param[in] newstate The state the IEEE802.15.4e FSM is now in.
*/
void changeState(ieee154e_state_t newstate) {
   // update the state
   ieee154e_vars.state = newstate;
   // wiggle the FSM debug pin
   switch (ieee154e_vars.state) {
      case S_SYNCLISTEN:
      case S_TXDATAOFFSET:
         debugpins_fsm_set();
         break;
      case S_SLEEP:
      case S_RXDATAOFFSET:
         debugpins_fsm_clr();
         break;
      case S_SYNCRX:
      case S_SYNCPROC:
      case S_TXDATAPREPARE:
      case S_TXDATAREADY:
      case S_TXDATADELAY:
      case S_TXDATA:
      case S_RXACKOFFSET:
      case S_RXACKPREPARE:
      case S_RXACKREADY:
      case S_RXACKLISTEN:
      case S_RXACK:
      case S_TXPROC:
      case S_RXDATAPREPARE:
      case S_RXDATAREADY:
      case S_RXDATALISTEN:
      case S_RXDATA:
      case S_TXACKOFFSET:
      case S_TXACKPREPARE:
      case S_TXACKREADY:
      case S_TXACKDELAY:
      case S_TXACK:
      case S_RXPROC:
         debugpins_fsm_toggle();
         break;
   }
}

/**
\brief Housekeeping tasks to do at the end of each slot.

This functions is called once in each slot, when there is nothing more
to do. This might be when an error occured, or when everything went well.
This function resets the state of the FSM so it is ready for the next slot.

Note that by the time this function is called, any received packet should already
have been sent to the upper layer. Similarly, in a Tx slot, the sendDone
function should already have been done. If this is not the case, this function
will do that for you, but assume that something went wrong.
*/
void endSlot() {
  
   // turn off the radio
   radio_rfOff();
   
   // compute the duty cycle if radio has been turned on
   if (ieee154e_vars.radioOnThisSlot==TRUE){  
      ieee154e_vars.radioOnTics+=(radio_getTimerValue()-ieee154e_vars.radioOnInit);
   }
   // clear any pending timer
   radiotimer_cancel();
   
   // reset capturedTimes
   ieee154e_vars.lastCapturedTime = 0;
   ieee154e_vars.syncCapturedTime = 0;
   
   //computing duty cycle.
   ieee154e_stats.numTicsOn+=ieee154e_vars.radioOnTics;//accumulate and tics the radio is on for that window
   ieee154e_stats.numTicsTotal+=radio_getTimerPeriod();//increment total tics by timer period.

   if (ieee154e_stats.numTicsTotal>DUTY_CYCLE_WINDOW_LIMIT){
      ieee154e_stats.numTicsTotal = ieee154e_stats.numTicsTotal>>1;
      ieee154e_stats.numTicsOn    = ieee154e_stats.numTicsOn>>1;
   }

   //clear vars for duty cycle on this slot   
   ieee154e_vars.radioOnTics=0;
   ieee154e_vars.radioOnThisSlot=FALSE;
   
   // clean up dataToSend
   if (ieee154e_vars.dataToSend!=NULL) {
      // if everything went well, dataToSend was set to NULL in ti9
      // getting here means transmit failed
      
      // indicate Tx fail to schedule to update stats
      schedule_indicateTx(&ieee154e_vars.asn,FALSE);
      
      //decrement transmits left counter
      ieee154e_vars.dataToSend->l2_retriesLeft--;
      
      if (ieee154e_vars.dataToSend->l2_retriesLeft==0) {
         // indicate tx fail if no more retries left
         notif_sendDone(ieee154e_vars.dataToSend,E_FAIL);
      } else {
         // return packet to the virtual COMPONENT_SIXTOP_TO_IEEE802154E component
         ieee154e_vars.dataToSend->owner = COMPONENT_SIXTOP_TO_IEEE802154E;
      }
      
      // reset local variable
      ieee154e_vars.dataToSend = NULL;
   }
   
   // clean up dataReceived
   if (ieee154e_vars.dataReceived!=NULL) {
      // assume something went wrong. If everything went well, dataReceived
      // would have been set to NULL in ri9.
      // indicate  "received packet" to upper layer since we don't want to loose packets
      notif_receive(ieee154e_vars.dataReceived);
      // reset local variable
      ieee154e_vars.dataReceived = NULL;
   }
   
   // clean up ackToSend
   if (ieee154e_vars.ackToSend!=NULL) {
      // free ackToSend so corresponding RAM memory can be recycled
      openqueue_freePacketBuffer(ieee154e_vars.ackToSend);
      // reset local variable
      ieee154e_vars.ackToSend = NULL;
   }
   
   // clean up ackReceived
   if (ieee154e_vars.ackReceived!=NULL) {
      // free ackReceived so corresponding RAM memory can be recycled
      openqueue_freePacketBuffer(ieee154e_vars.ackReceived);
      // reset local variable
      ieee154e_vars.ackReceived = NULL;
   }
   
   
   // change state
   changeState(S_SLEEP);
}

bool ieee154e_isSynch(){
   return ieee154e_vars.isSync;
}
