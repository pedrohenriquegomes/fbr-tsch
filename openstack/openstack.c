/**
\brief Entry point for accessing the OpenWSN stack.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, October 2014.
*/

#include "opendefs.h"
//===== drivers
#include "openserial.h"
//===== stack
#include "openstack.h"
//-- cross-layer
#include "idmanager.h"
#include "openqueue.h"
#include "openrandom.h"
#include "opentimers.h"
//-- 02a-TSCH
#include "adaptive_sync.h"
#include "IEEE802154E.h"
//-- 02b-RES
#include "schedule.h"
#include "sixtop.h"
#include "neighbors.h"
//===== applications
#include "openapps.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

//=========================== private =========================================

void openstack_init(void) {
   
   //===== drivers
   openserial_init();
   
   //===== stack
   //-- cross-layer
   idmanager_init();    // call first since initializes EUI64 and isDAGroot
   openqueue_init();
   openrandom_init();
   opentimers_init();
   //-- 02a-TSCH
   adaptive_sync_init();
   ieee154e_init();
   //-- 02b-RES
   schedule_init();
   sixtop_init();
   neighbors_init();
   
   //===== applications
   openapps_init();
   
   openserial_printInfo(
      COMPONENT_OPENWSN,ERR_BOOTED,
      (errorparameter_t)0,
      (errorparameter_t)0
   );
}
