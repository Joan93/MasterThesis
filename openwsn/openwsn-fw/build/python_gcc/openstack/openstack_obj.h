/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:08:13.899030.
*/
/**
\brief Entry point for accessing the OpenWSN stack.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, October 2014.
*/

#ifndef __OPENSTACK_H
#define __OPENSTACK_H

#include "Python.h"

#include "opendefs_obj.h"

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

#include "openwsnmodule_obj.h"
typedef struct OpenMote OpenMote;

//=========================== prototypes ======================================

void openstack_init(OpenMote* self);

#endif
