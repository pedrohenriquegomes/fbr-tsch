#ifndef __NEIGHBORS_H
#define __NEIGHBORS_H

/**
\addtogroup MAChigh
\{
\addtogroup Neighbors
\{
*/
#include "opendefs.h"

//=========================== define ==========================================

#define MAXNUMNEIGHBORS           15
#define AVERAGEDEGREE             6
#define MAXPREFERENCE             2
#define BADNEIGHBORMAXRSSI        -70 // dBm
#define GOODNEIGHBORMINRSSI       -80 // dBm
#define SWITCHSTABILITYTHRESHOLD  3
#define DEFAULTLINKCOST           15

#define MAXDAGRANK                0xff
#define DEFAULTDAGRANK            MAXDAGRANK
#define MINHOPRANKINCREASE        1

//=========================== typedef =========================================

BEGIN_PACK
typedef struct {
   bool            used;
   uint8_t         parentPreference;
   bool            stableNeighbor;
   uint8_t         switchStabilityCounter;
   uint16_t        shortID;
   dagrank_t       DAGrank;
   int8_t          rssi;
   uint8_t         numRx;
   uint8_t         numTx;
   uint8_t         numWraps; //number of times the tx counter wraps. can be removed if memory is a restriction. also check openvisualizer then.
   asn_t           asn;
} neighborRow_t;
END_PACK

BEGIN_PACK
typedef struct {
   uint8_t         row;
   neighborRow_t   neighborEntry;
} debugNeighborEntry_t;
END_PACK

BEGIN_PACK
typedef struct {
   uint8_t         last_addr_byte;   // last byte of the neighbor's address
   int8_t          rssi;
   uint8_t         parentPreference;
   dagrank_t       DAGrank;
   uint16_t        asn; 
} netDebugNeigborEntry_t;
END_PACK

//=========================== module variables ================================
   
typedef struct {
   neighborRow_t        neighbors[MAXNUMNEIGHBORS];
   dagrank_t            myDAGrank;
   uint8_t              debugRow;
} neighbors_vars_t;

//=========================== prototypes ======================================

void          neighbors_init(void);

// getters
dagrank_t     neighbors_getMyDAGrank(void);
uint8_t       neighbors_getNumNeighbors(void);

// interrogators
bool          neighbors_isStableNeighbor(uint16_t shortID);
bool          neighbors_isPreferredParent(uint16_t shortID);

// updating neighbor information
void          neighbors_indicateRx(
   uint16_t             src,
   int8_t               rssi,
   asn_t*               asnTs
);
void          neighbors_indicateRxEB(
   OpenQueueEntry_t*    msg
);

// managing routing info
void          neighbors_updateMyDAGrankAndNeighborPreference(void);
// debug
bool          debugPrint_neighbors(void);

/**
\}
\}
*/

#endif
