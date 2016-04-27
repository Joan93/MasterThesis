/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:10:29.178933.
*/
#ifndef __UINJECT_H
#define __UINJECT_H

/**
\addtogroup AppUdp
\{
\addtogroup uinject
\{
*/

#include "Python.h"

#include "opentimers_obj.h"

//=========================== define ==========================================

#define UINJECT_PERIOD_MS 30000

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   opentimer_id_t       timerId;  ///< periodic timer which triggers transmission
   uint16_t             counter;  ///< incrementing counter which is written into the packet
} uinject_vars_t;

#include "openwsnmodule_obj.h"
typedef struct OpenMote OpenMote;

//=========================== prototypes ======================================

void uinject_init(OpenMote* self);
void uinject_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error);
void uinject_receive(OpenMote* self, OpenQueueEntry_t* msg);

/**
\}
\}
*/

#endif
