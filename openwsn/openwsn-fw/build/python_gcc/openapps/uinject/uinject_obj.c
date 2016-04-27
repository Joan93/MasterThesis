/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:10:49.450322.
*/
#include "opendefs_obj.h"
#include "uinject_obj.h"
#include "openudp_obj.h"
#include "openqueue_obj.h"
#include "opentimers_obj.h"
#include "openserial_obj.h"
#include "packetfunctions_obj.h"
#include "scheduler_obj.h"
#include "IEEE802154E_obj.h"
#include "idmanager_obj.h"

//=========================== variables =======================================

uinject_vars_t uinject_vars;

static const uint8_t uinject_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
}; 

//=========================== prototypes ======================================

void uinject_timer_cb(OpenMote* self, opentimer_id_t id);
void uinject_task_cb(OpenMote* self);

//=========================== public ==========================================

void uinject_init(OpenMote* self) {
   
   // clear local variables
   memset(&uinject_vars,0,sizeof(uinject_vars_t));
   
   // start periodic timer
   uinject_vars.timerId                    = opentimers_start(self, 
      UINJECT_PERIOD_MS,
      TIMER_PERIODIC,TIME_MS,
      uinject_timer_cb
   );
}

void uinject_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error) {
 openqueue_freePacketBuffer(self, msg);
}

void uinject_receive(OpenMote* self, OpenQueueEntry_t* pkt) {
   
 openqueue_freePacketBuffer(self, pkt);
   
 openserial_printError(self, 
      COMPONENT_UINJECT,
      ERR_RCVD_ECHO_REPLY,
      (errorparameter_t)0,
      (errorparameter_t)0
   );
}

//=========================== private =========================================

/**
\note timer fired, but we don't want to execute task in ISR mode instead, push
   task to scheduler with CoAP priority, and let scheduler take care of it.
*/
void uinject_timer_cb(OpenMote* self, opentimer_id_t id){
   
 scheduler_push_task(self, uinject_task_cb,TASKPRIO_COAP);
}

void uinject_task_cb(OpenMote* self) {
   OpenQueueEntry_t*    pkt;
   
   // don't run if not synch
   if ( ieee154e_isSynch(self) == FALSE) return;
   
   // don't run on dagroot
   if ( idmanager_getIsDAGroot(self)) {
 opentimers_stop(self, uinject_vars.timerId);
      return;
   }
   
   // if you get here, send a packet
   
   // get a free packet buffer
   pkt = openqueue_getFreePacketBuffer(self, COMPONENT_UINJECT);
   if (pkt==NULL) {
 openserial_printError(self, 
         COMPONENT_UINJECT,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   pkt->owner                         = COMPONENT_UINJECT;
   pkt->creator                       = COMPONENT_UINJECT;
   pkt->l4_protocol                   = IANA_UDP;
   pkt->l4_destination_port           = WKP_UDP_INJECT;
   pkt->l4_sourcePortORicmpv6Type     = WKP_UDP_INJECT;
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],uinject_dst_addr,16);
   
 packetfunctions_reserveHeaderSize(self, pkt,sizeof(uint16_t));
   *((uint16_t*)&pkt->payload[0]) = uinject_vars.counter++;
   
   if (( openudp_send(self, pkt))==E_FAIL) {
 openqueue_freePacketBuffer(self, pkt);
   }
}