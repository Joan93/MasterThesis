/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:08:36.140173.
*/
#ifndef __OPENRANDOM_H
#define __OPENRANDOM_H

/**
\addtogroup cross-layers
\{
\addtogroup OpenRandom
\{
*/

#include "Python.h"

#include "opendefs_obj.h"

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== module variables ================================

typedef struct {
   uint16_t shift_reg;  // Galois shift register used to obtain a pseudo-random number
} random_vars_t;

#include "openwsnmodule_obj.h"
typedef struct OpenMote OpenMote;

//=========================== prototypes ======================================

void openrandom_init(OpenMote* self);
uint16_t openrandom_get16b(OpenMote* self);

/**
\}
\}
*/

#endif
