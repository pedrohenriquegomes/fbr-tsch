#ifndef __PROCESSIE_H
#define __PROCESSIE_H

#include "opendefs.h"

//=========================== define ==========================================

// maximum of cells in a Schedule IE
#define SCHEDULEIEMAXNUMCELLS 3

// subIE shift
#define MLME_IE_SUBID_SHIFT            8

// subIEs identifier
#define MLME_IE_SUBID_SYNC             0x1A
#define MLME_IE_SUBID_SLOTFRAME_LINK   0x1B
#define MLME_IE_SUBID_TIMESLOT         0x1c
#define MLME_IE_SUBID_CHANNELHOPPING   0x09
#define MLME_IE_SUBID_LINKTYPE         0x40
#define MLME_IE_SUBID_OPCODE           0x41
#define MLME_IE_SUBID_BANDWIDTH        0x42
#define MLME_IE_SUBID_TRACKID          0x43
#define MLME_IE_SUBID_SCHEDULE         0x44
// ========================== typedef =========================================

BEGIN_PACK

typedef struct {
   uint16_t        tsNum;
   uint16_t        choffset;
   uint8_t         linkoptions;
} cellInfo_ht;

/**
\brief Header of header IEs.
*/
typedef struct{
   uint16_t length_elementid_type; 
} header_IE_ht; 

/**
\brief Header of payload IEs.
*/
typedef struct{
   uint16_t length_groupid_type;
} payload_IE_ht;

//======= header IEs

//======= payload IEs

/**
\brief TSCH Synchronization IE

http://tools.ietf.org/html/draft-wang-6tisch-6top-sublayer-01#section-4.1.1.1
*/
typedef struct {
   uint8_t         asn[5];
   uint8_t         join_priority;
} sync_IE_ht;

END_PACK

//=========================== variables =======================================

//=========================== prototypes ======================================

//===== prepend IEs
uint8_t          processIE_prependCounterIE(
   OpenQueueEntry_t*    pkt
);
uint8_t          processIE_prependSyncIE(
   OpenQueueEntry_t*    pkt
);

#endif
