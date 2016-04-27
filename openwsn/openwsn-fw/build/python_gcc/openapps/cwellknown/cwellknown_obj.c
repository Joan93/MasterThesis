/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:10:44.508340.
*/
#include "opendefs_obj.h"
#include "cwellknown_obj.h"
#include "opencoap_obj.h"
#include "openqueue_obj.h"
#include "packetfunctions_obj.h"
#include "openserial_obj.h"
#include "idmanager_obj.h"

//=========================== variables =======================================

cwellknown_vars_t cwellknown_vars;

const uint8_t cwellknown_path0[]       = ".well-known";
const uint8_t cwellknown_path1[]       = "core";

//=========================== prototypes ======================================

owerror_t cwellknown_receive(OpenMote* self, 
   OpenQueueEntry_t* msg,
   coap_header_iht*  coap_header,
   coap_option_iht*  coap_options
);

void cwellknown_sendDone(OpenMote* self, 
   OpenQueueEntry_t* msg,
   owerror_t         error
);

//=========================== public ==========================================

void cwellknown_init(OpenMote* self) {
   if( idmanager_getIsDAGroot(self)==TRUE) return; 
   
   // prepare the resource descriptor for the /.well-known/core path
   cwellknown_vars.desc.path0len            = sizeof(cwellknown_path0)-1;
   cwellknown_vars.desc.path0val            = (uint8_t*)(&cwellknown_path0);
   cwellknown_vars.desc.path1len            = sizeof(cwellknown_path1)-1;
   cwellknown_vars.desc.path1val            = (uint8_t*)(&cwellknown_path1);
   cwellknown_vars.desc.componentID         = COMPONENT_CWELLKNOWN;
   cwellknown_vars.desc.discoverable        = FALSE;
   cwellknown_vars.desc.callbackRx          = &cwellknown_receive;
   cwellknown_vars.desc.callbackSendDone    = &cwellknown_sendDone;
   
 opencoap_register(self, &cwellknown_vars.desc);
}

//=========================== private =========================================

owerror_t cwellknown_receive(OpenMote* self, 
      OpenQueueEntry_t* msg,
      coap_header_iht*  coap_header,
      coap_option_iht*  coap_options
   ) {
   owerror_t outcome;
   
   switch(coap_header->Code) {
      case COAP_CODE_REQ_GET:
         // reset packet payload
         msg->payload        = &(msg->packet[127]);
         msg->length         = 0;
         
         // have CoAP module write links to all resources
 opencoap_writeLinks(self, msg,COMPONENT_CWELLKNOWN);
         
 packetfunctions_reserveHeaderSize(self, msg,1);
         msg->payload[0]     = COAP_PAYLOAD_MARKER;
            
         // add return option
 packetfunctions_reserveHeaderSize(self, msg,2);
         msg->payload[0]     = COAP_OPTION_NUM_CONTENTFORMAT << 4 | 1;
         msg->payload[1]     = COAP_MEDTYPE_APPLINKFORMAT;
         
         // set the CoAP header
         coap_header->Code   = COAP_CODE_RESP_CONTENT;
         
         outcome             = E_SUCCESS;
         
         break;
      default:
         outcome             = E_FAIL;
         break;
   }
   
   return outcome;
}

void cwellknown_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error) {
 openqueue_freePacketBuffer(self, msg);
}
