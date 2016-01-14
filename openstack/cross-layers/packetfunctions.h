#ifndef __PACKETFUNCTIONS_H
#define __PACKETFUNCTIONS_H

/**
\addtogroup cross-layers
\{
\addtogroup PacketFunctions
\{
*/

#include "opendefs.h"

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

//=========================== prototypes ======================================

// address translation
void     packetfunctions_mac64bToMac16b(open_addr_t* mac64b, open_addr_t* mac16btoWrite);

// address recognition
bool     packetfunctions_isBroadcastMulticast(open_addr_t* address);
bool     packetfunctions_sameAddress(open_addr_t* address_1, open_addr_t* address_2);

// read/write addresses to/from packets
void     packetfunctions_readAddress(uint8_t* payload, uint8_t type, open_addr_t* writeToAddress, bool littleEndian);
void     packetfunctions_writeAddress(OpenQueueEntry_t* msg, open_addr_t* address, bool littleEndian);

// reserving/tossing headers and footers
void     packetfunctions_reserveHeaderSize(OpenQueueEntry_t* pkt, uint8_t header_length);
void     packetfunctions_tossHeader(OpenQueueEntry_t* pkt, uint8_t header_length);
void     packetfunctions_reserveFooterSize(OpenQueueEntry_t* pkt, uint8_t header_length);
void     packetfunctions_tossFooter(OpenQueueEntry_t* pkt, uint8_t header_length);

// packet duplication
void packetfunctions_duplicatePacket(OpenQueueEntry_t* dst, OpenQueueEntry_t* src);

/**
\}
\}
*/

#endif
