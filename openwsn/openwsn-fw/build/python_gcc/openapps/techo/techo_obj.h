/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:08:42.572254.
*/
#ifndef __TECHO_H
#define __TECHO_H

/**
\addtogroup AppTcp
\{
\addtogroup techo
\{
*/

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

#include "openwsnmodule_obj.h"
typedef struct OpenMote OpenMote;

//=========================== prototypes ======================================

void techo_init(OpenMote* self);
bool techo_shouldIlisten(OpenMote* self);
void techo_receive(OpenMote* self, OpenQueueEntry_t* msg);
void techo_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error);
void techo_connectDone(OpenMote* self, owerror_t error);
bool techo_debugPrint(OpenMote* self);

/**
\}
\}
*/

#endif
