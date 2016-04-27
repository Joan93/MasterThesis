/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:10:37.643373.
*/
/**
\brief Applications running on top of the OpenWSN stack.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, September 2014.
*/

#include "opendefs_obj.h"

// CoAP
#include "c6t_obj.h"
#include "cinfo_obj.h"
#include "cleds_obj.h"
#include "cexample_obj.h"
#include "cstorm_obj.h"
#include "cwellknown_obj.h"
#include "rrt_obj.h"
// TCP
#include "techo_obj.h"
// UDP
#include "uecho_obj.h"
#include "uinject_obj.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

//=========================== private =========================================

void openapps_init(OpenMote* self) {
   // CoAP
 c6t_init(self);
 cinfo_init(self);
   // cexample_init(self);
 cleds__init(self);
 cstorm_init(self);
 cwellknown_init(self);
 rrt_init(self);
   // TCP
 techo_init(self);
   // UDP
 uecho_init(self);
   // uinject_init(self);
}
