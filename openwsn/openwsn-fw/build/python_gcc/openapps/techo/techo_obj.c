/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:10:47.795123.
*/
#include "opendefs_obj.h"
#include "techo_obj.h"
#include "openqueue_obj.h"
#include "openserial_obj.h"
#include "opentcp_obj.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

void techo_init(OpenMote* self) {
}

bool techo_shouldIlisten(OpenMote* self) {
   return TRUE;
}

void techo_receive(OpenMote* self, OpenQueueEntry_t* msg) {
   uint16_t temp_l4_destination_port;
   msg->owner                     = COMPONENT_TECHO;
   //reply with the same OpenQueueEntry_t
   msg->creator                   = COMPONENT_TECHO;
   msg->l4_protocol               = IANA_TCP;
   temp_l4_destination_port       = msg->l4_destination_port;
   msg->l4_destination_port       = msg->l4_sourcePortORicmpv6Type;
   msg->l4_sourcePortORicmpv6Type = temp_l4_destination_port;
   if ( opentcp_send(self, msg)==E_FAIL) {
 openqueue_freePacketBuffer(self, msg);
   }
}

void techo_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error) {
   msg->owner = COMPONENT_TECHO;
   if (msg->creator!=COMPONENT_TECHO) {
 openserial_printError(self, COMPONENT_TECHO,ERR_UNEXPECTED_SENDDONE,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
   }
   //close TCP session, but keep listening
 opentcp_close(self);
 openqueue_freePacketBuffer(self, msg);
}

void techo_connectDone(OpenMote* self, owerror_t error) {
}

bool techo_debugPrint(OpenMote* self) {
   return FALSE;
}

//=========================== private =========================================
