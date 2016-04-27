/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:08:46.691080.
*/
/**
\brief Entry point for accessing the OpenWSN stack.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, October 2014.
*/

#include "opendefs_obj.h"
//===== drivers
#include "openserial_obj.h"
//===== stack
#include "openstack_obj.h"
//-- cross-layer
#include "idmanager_obj.h"
#include "openqueue_obj.h"
#include "openrandom_obj.h"
#include "opentimers_obj.h"
//-- 02a-TSCH
#include "adaptive_sync_obj.h"
#include "IEEE802154E_obj.h"
#include "ieee802154_security_driver_obj.h"
//-- 02b-RES
#include "schedule_obj.h"
#include "sixtop_obj.h"
#include "neighbors_obj.h"
//-- 03a-IPHC
#include "openbridge_obj.h"
#include "iphc_obj.h"
//-- 03b-IPv6
#include "forwarding_obj.h"
#include "icmpv6_obj.h"
#include "icmpv6echo_obj.h"
#include "icmpv6rpl_obj.h"
//-- 04-TRAN
#include "opentcp_obj.h"
#include "openudp_obj.h"
#include "opencoap_obj.h"
//===== applications
#include "openapps_obj.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

//=========================== private =========================================

void openstack_init(OpenMote* self) {
   
   //===== drivers
 openserial_init(self);
   
   //===== stack
   //-- cross-layer
 idmanager_init(self);    // call first since initializes EUI64 and isDAGroot
 openqueue_init(self);
 openrandom_init(self);
 opentimers_init(self);
   //-- 02a-TSCH
 adaptive_sync_init(self);
 ieee154e_init(self);
   IEEE802154_SECURITY.init();
   //-- 02b-RES
 schedule_init(self);
 sixtop_init(self);
 neighbors_init(self);
   //-- 03a-IPHC
 openbridge_init(self);
 iphc_init(self);
   //-- 03b-IPv6
 forwarding_init(self);
 icmpv6_init(self);
 icmpv6echo_init(self);
 icmpv6rpl_init(self);
   //-- 04-TRAN
 opentcp_init(self);
 openudp_init(self);
 opencoap_init(self);     // initialize before any of the CoAP applications
   
   //===== applications
 openapps_init(self);
   
 openserial_printInfo(self, 
      COMPONENT_OPENWSN,ERR_BOOTED,
      (errorparameter_t)0,
      (errorparameter_t)0
   );
}
