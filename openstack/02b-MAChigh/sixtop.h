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

enum sixtop_CommandID_num{
   SIXTOP_SOFT_CELL_REQ                = 0x00,
   SIXTOP_SOFT_CELL_RESPONSE           = 0x01,
   SIXTOP_REMOVE_SOFT_CELL_REQUEST     = 0x02,
};

// states of the sixtop-to-sixtop state machine
typedef enum {
   SIX_IDLE                            = 0x00,   // ready for next event
   // ADD: source
   SIX_SENDING_ADDREQUEST              = 0x01,   // generating LinkRequest packet
   SIX_WAIT_ADDREQUEST_SENDDONE        = 0x02,   // waiting for SendDone confirmation
   SIX_WAIT_ADDRESPONSE                = 0x03,   // waiting for response from the neighbor
   SIX_ADDRESPONSE_RECEIVED            = 0x04,   // I received the link response request command
   // ADD: destinations
   SIX_ADDREQUEST_RECEIVED             = 0x05,   // I received the link request command
   SIX_SENDING_ADDRESPONSE             = 0x06,   // generating resLinkRespone command packet
   SIX_WAIT_ADDRESPONSE_SENDDONE       = 0x07,   // waiting for SendDone confirmation
   // REMOVE: source
   SIX_SENDING_REMOVEREQUEST           = 0x08,   // generating resLinkRespone command packet
   SIX_WAIT_REMOVEREQUEST_SENDDONE     = 0x09,   // waiting for SendDone confirmation
   // REMOVE: destinations
   SIX_REMOVEREQUEST_RECEIVED          = 0x0a    // I received the remove link request command
} six2six_state_t;

// before sixtop protocol is called, sixtop handler must be set
typedef enum {
   SIX_HANDLER_NONE                    = 0x00, // when complete reservation, handler must be set to none
   SIX_HANDLER_MAINTAIN                = 0x01, // the handler is maintenance process
   SIX_HANDLER_OTF                     = 0x02  // the handler is otf
} six2six_handler_t;

BEGIN_PACK
typedef struct {                                 // always written big endian, i.e. MSB in addr[0]
   uint16_t  type;
   uint16_t  src;
   uint8_t   syncnum;
   uint8_t   ebrank;
   uint8_t   asn0;
   uint8_t   asn1;
   uint8_t   asn2;
   uint8_t   asn3;
   uint8_t   light_info;
} eb_ht;
END_PACK

//=========================== typedef =========================================

#define SIX2SIX_TIMEOUT_MS 4000
#define SIXTOP_MINIMAL_EBPERIOD 5 // minist period of sending EB

//=========================== module variables ================================

//=========================== prototypes ======================================

// admin
void      sixtop_init(void);
// from upper layer
owerror_t sixtop_send(OpenQueueEntry_t *msg);
// from lower layer
void      sixtop_sendEB(void);
void      task_sixtopNotifSendDone(void);
void      task_sixtopNotifReceive(void);
// debugging
bool      debugPrint_myDAGrank(void);

/**
\}
\}
*/

#endif
