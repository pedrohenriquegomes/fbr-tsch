#include "packetfunctions.h"
#include "openserial.h"
#include "idmanager.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

void onesComplementSum(uint8_t* global_sum, uint8_t* ptr, int length);

//=========================== public ==========================================

//======= address translation

//assuming an ip128b is a concatenation of prefix64b followed by a mac64b
void packetfunctions_ip128bToMac64b(
      open_addr_t* ip128b,
      open_addr_t* prefix64btoWrite,
      open_addr_t* mac64btoWrite) {
   if (ip128b->type!=ADDR_128B) {
      openserial_printCritical(COMPONENT_PACKETFUNCTIONS,ERR_WRONG_ADDR_TYPE,
                            (errorparameter_t)ip128b->type,
                            (errorparameter_t)0);
      mac64btoWrite->type=ADDR_NONE;
      return;
   }
   prefix64btoWrite->type=ADDR_PREFIX;
   memcpy(prefix64btoWrite->prefix, &(ip128b->addr_128b[0]), 8);
   mac64btoWrite->type=ADDR_64B;
   memcpy(mac64btoWrite->addr_64b , &(ip128b->addr_128b[8]), 8);
}

//assuming an mac16b is lower 2B of mac64b
void packetfunctions_mac64bToMac16b(open_addr_t* mac64b, open_addr_t* mac16btoWrite) {
   if (mac64b->type!=ADDR_64B) {
      openserial_printCritical(COMPONENT_PACKETFUNCTIONS,ERR_WRONG_ADDR_TYPE,
                            (errorparameter_t)mac64b->type,
                            (errorparameter_t)2);
      mac16btoWrite->type=ADDR_NONE;
      return;
   }
   mac16btoWrite->type = ADDR_16B;
   mac16btoWrite->addr_16b[0] = mac64b->addr_64b[6];
   mac16btoWrite->addr_16b[1] = mac64b->addr_64b[7];
}

//======= address recognition

bool packetfunctions_isBroadcastMulticast(open_addr_t* address) {
   uint8_t i;
   uint8_t address_length;
   //IPv6 multicast
   if (address->type==ADDR_128B) {
      if (address->addr_128b[0]==0xff) {
         return TRUE;
      } else {
         return FALSE;
      }
   }
   //15.4 broadcast
   switch (address->type) {
      case ADDR_16B:
         address_length = 2;
         break;
      case ADDR_64B:
         address_length = 8;
         break;
      default:
         openserial_printCritical(COMPONENT_PACKETFUNCTIONS,ERR_WRONG_ADDR_TYPE,
                               (errorparameter_t)address->type,
                               (errorparameter_t)4);
         return FALSE;
   }
   for (i=0;i<address_length;i++) {
      if (address->addr_128b[i]!=0xFF) {
         return FALSE;
      }
   }
   return TRUE;
}

bool packetfunctions_sameAddress(open_addr_t* address_1, open_addr_t* address_2) {
   uint8_t address_length;
   
   if (address_1->type!=address_2->type) {
      return FALSE;
   }
   switch (address_1->type) {
      case ADDR_16B:
      case ADDR_PANID:
         address_length = 2;
         break;
      case ADDR_64B:
      case ADDR_PREFIX:
         address_length = 8;
         break;
      case ADDR_128B:
      case ADDR_ANYCAST:
         address_length = 16;
         break;
    
      default:
         openserial_printCritical(COMPONENT_PACKETFUNCTIONS,ERR_WRONG_ADDR_TYPE,
                               (errorparameter_t)address_1->type,
                               (errorparameter_t)5);
         return FALSE;
   }
   if (memcmp((void*)address_1->addr_128b,(void*)address_2->addr_128b,address_length)==0) {
      return TRUE;
   }
   return FALSE;
}

//======= address read/write

void packetfunctions_readAddress(uint8_t* payload, uint8_t type, open_addr_t* writeToAddress, bool littleEndian) {
   uint8_t i;
   uint8_t address_length;
   
   writeToAddress->type = type;
   switch (type) {
      case ADDR_16B:
      case ADDR_PANID:
         address_length = 2;
         break;
      case ADDR_64B:
      case ADDR_PREFIX:
         address_length = 8;
         break;
      case ADDR_128B:
         address_length = 16;
         break;
      default:
         openserial_printCritical(COMPONENT_PACKETFUNCTIONS,ERR_WRONG_ADDR_TYPE,
                               (errorparameter_t)type,
                               (errorparameter_t)6);
         return;
   }
   
   for (i=0;i<address_length;i++) {
      if (littleEndian) {
         writeToAddress->addr_128b[address_length-1-i] = *(payload+i);
      } else {
         writeToAddress->addr_128b[i]   = *(payload+i);
      }
   }
}

void packetfunctions_writeAddress(OpenQueueEntry_t* msg, open_addr_t* address, bool littleEndian) {
   uint8_t i;
   uint8_t address_length;
   
   switch (address->type) {
      case ADDR_16B:
      case ADDR_PANID:
         address_length = 2;
         break;
      case ADDR_64B:
      case ADDR_PREFIX:
         address_length = 8;
         break;
      case ADDR_128B:
         address_length = 16;
         break;
      default:
         openserial_printCritical(COMPONENT_PACKETFUNCTIONS,ERR_WRONG_ADDR_TYPE,
                               (errorparameter_t)address->type,
                               (errorparameter_t)7);
         return;
   }
   
   for (i=0;i<address_length;i++) {
      msg->payload      -= sizeof(uint8_t);
      msg->length       += sizeof(uint8_t);
      if (littleEndian) {
         *((uint8_t*)(msg->payload)) = address->addr_128b[i];
      } else {
         *((uint8_t*)(msg->payload)) = address->addr_128b[address_length-1-i];
      }
   }
}

//======= reserving/tossing headers

void packetfunctions_reserveHeaderSize(OpenQueueEntry_t* pkt, uint8_t header_length) {
   pkt->payload -= header_length;
   pkt->length  += header_length;
   if ( (uint8_t*)(pkt->payload) < (uint8_t*)(pkt->packet) ) {
      openserial_printCritical(COMPONENT_PACKETFUNCTIONS,ERR_HEADER_TOO_LONG,
                            (errorparameter_t)0,
                            (errorparameter_t)pkt->length);
   }
}

void packetfunctions_tossHeader(OpenQueueEntry_t* pkt, uint8_t header_length) {
   pkt->payload += header_length;
   pkt->length  -= header_length;
   if ( (uint8_t*)(pkt->payload) > (uint8_t*)(pkt->packet+126) ) {
      openserial_printError(COMPONENT_PACKETFUNCTIONS,ERR_HEADER_TOO_LONG,
                            (errorparameter_t)1,
                            (errorparameter_t)pkt->length);
   }
}

void packetfunctions_reserveFooterSize(OpenQueueEntry_t* pkt, uint8_t header_length) {
   pkt->length  += header_length;
   if (pkt->length>127) {
      openserial_printError(COMPONENT_PACKETFUNCTIONS,ERR_HEADER_TOO_LONG,
                            (errorparameter_t)2,
                            (errorparameter_t)pkt->length);
   }
}

void packetfunctions_tossFooter(OpenQueueEntry_t* pkt, uint8_t header_length) {
   pkt->length  -= header_length;
   if (pkt->length>128) {//wraps around, so a negative value will be >128
      openserial_printError(COMPONENT_PACKETFUNCTIONS,ERR_HEADER_TOO_LONG,
                            (errorparameter_t)3,
                            (errorparameter_t)pkt->length);
   }
}

//======= packet duplication
// function duplicates a frame from one OpenQueueEntry structure to the other,
// updating pointers to the new memory location. Used to make a local copy of
// the frame before transmission (where it can possibly be encrypted). 
void packetfunctions_duplicatePacket(OpenQueueEntry_t* dst, OpenQueueEntry_t* src) {
   // make a copy of the frame
   memcpy(dst, src, sizeof(OpenQueueEntry_t));

   // Calculate where payload starts in the buffer
   dst->payload = &dst->packet[src->payload - src->packet]; // update pointers

   // update l2_FrameCounter pointer
//   dst->l2_FrameCounter = dst->payload + (src->l2_FrameCounter - src->payload);

   // update l2_ASNpayload pointer
   dst->l2_ASNpayload = dst->payload + (src->l2_ASNpayload - src->payload);

   // update l2_scheduleIE_cellObjects pointer
   dst->l2_scheduleIE_cellObjects = dst->payload + (src->l2_scheduleIE_cellObjects - src->payload);

   // update l2_payload pointer
   dst->l2_payload = dst->payload + (src->l2_payload - src->payload);

   // update l4_payload pointer
   dst->l4_payload = dst->payload + (src->l4_payload - src->payload);
}

//=========================== private =========================================
