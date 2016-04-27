/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:09:44.099240.
*/
#include "opendefs_obj.h"
#include "sixtop_obj.h"
#include "openserial_obj.h"
#include "openqueue_obj.h"
#include "neighbors_obj.h"
#include "IEEE802154E_obj.h"
#include "iphc_obj.h"
#include "otf_obj.h"
#include "packetfunctions_obj.h"
#include "openrandom_obj.h"
#include "scheduler_obj.h"
#include "opentimers_obj.h"
#include "debugpins_obj.h"
#include "leds_obj.h"
#include "processIE_obj.h"
#include "IEEE802154_obj.h"
#include "IEEE802154_security_obj.h"
#include "idmanager_obj.h"
#include "schedule_obj.h"

//=========================== variables =======================================

// declaration of global variable _sixtop_vars_ removed during objectification.

//=========================== prototypes ======================================

// send internal
owerror_t sixtop_send_internal(OpenMote* self, 
   OpenQueueEntry_t*    msg,
   bool                 payloadIEPresent
);

// timer interrupt callbacks
void sixtop_maintenance_timer_cb(OpenMote* self, opentimer_id_t id);
void sixtop_timeout_timer_cb(OpenMote* self, opentimer_id_t id);

//=== EB/KA task

void timer_sixtop_management_fired(OpenMote* self);
void sixtop_sendEB(OpenMote* self);
void sixtop_sendKA(OpenMote* self);

//=== six2six task

void timer_sixtop_six2six_timeout_fired(OpenMote* self);
void sixtop_six2six_sendDone(OpenMote* self, 
   OpenQueueEntry_t*    msg,
   owerror_t            error
);
bool sixtop_processIEs(OpenMote* self, 
   OpenQueueEntry_t*    pkt,
   uint16_t*            lenIE
);
void sixtop_notifyReceiveCommand(OpenMote* self, 
   uint8_t              version, 
   uint8_t              commandId, 
   uint8_t              sfId,
   uint8_t              ptr,
   uint8_t              length,
   OpenQueueEntry_t*    pkt
);

uint8_t sixtop_getCelllist(OpenMote* self, 
   uint8_t             frameID,
   open_addr_t*         neighbor,
   cellInfo_ht*         cellList
);

//=== helper functions

bool sixtop_candidateAddCellList(OpenMote* self, 
   uint8_t*             frameID,
   cellInfo_ht*         cellList,
   uint8_t              requiredCells
);
bool sixtop_candidateRemoveCellList(OpenMote* self, 
   uint8_t*             frameID,
   cellInfo_ht*         cellList,
   open_addr_t*         neighbor,
   uint8_t              requiredCells
);
void sixtop_addCellsByState(OpenMote* self, 
   uint8_t              slotframeID,
   cellInfo_ht*         cellList,
   open_addr_t*         previousHop,
   uint8_t              state
);
void sixtop_removeCellsByState(OpenMote* self, 
   uint8_t              slotframeID,
   cellInfo_ht*         cellList,
   open_addr_t*         previousHop
);
bool sixtop_areAvailableCellsToBeScheduled(OpenMote* self, 
    uint8_t              frameID, 
    uint8_t              numOfCells, 
    cellInfo_ht*         cellList
);
bool sixtop_areAvailableCellsToBeRemoved(OpenMote* self,       
    uint8_t      frameID, 
    uint8_t      numOfCells, 
    cellInfo_ht* cellList,
    open_addr_t* neighbor
);

//=========================== public ==========================================

void sixtop_init(OpenMote* self) {
   
   (self->sixtop_vars).periodMaintenance  = 872 +( openrandom_get16b(self)&0xff);
   (self->sixtop_vars).busySendingKA      = FALSE;
   (self->sixtop_vars).busySendingEB      = FALSE;
   (self->sixtop_vars).dsn                = 0;
   (self->sixtop_vars).mgtTaskCounter     = 0;
   (self->sixtop_vars).kaPeriod           = MAXKAPERIOD;
   (self->sixtop_vars).ebPeriod           = EBPERIOD;
   (self->sixtop_vars).isResponseEnabled  = TRUE;
   
   (self->sixtop_vars).maintenanceTimerId = opentimers_start(self, 
      (self->sixtop_vars).periodMaintenance,
      TIMER_PERIODIC,
      TIME_MS,
      sixtop_maintenance_timer_cb
   );
   
   (self->sixtop_vars).timeoutTimerId     = opentimers_start(self, 
      SIX2SIX_TIMEOUT_MS,
      TIMER_ONESHOT,
      TIME_MS,
      sixtop_timeout_timer_cb
   );
}

void sixtop_setKaPeriod(OpenMote* self, uint16_t kaPeriod) {
   if(kaPeriod > MAXKAPERIOD) {
      (self->sixtop_vars).kaPeriod = MAXKAPERIOD;
   } else {
      (self->sixtop_vars).kaPeriod = kaPeriod;
   } 
}

void sixtop_setEBPeriod(OpenMote* self, uint8_t ebPeriod) {
   if(ebPeriod < SIXTOP_MINIMAL_EBPERIOD) {
      (self->sixtop_vars).ebPeriod = SIXTOP_MINIMAL_EBPERIOD;
   } else {
      (self->sixtop_vars).ebPeriod = ebPeriod;
   } 
}

void sixtop_setHandler(OpenMote* self, six2six_handler_t handler) {
    (self->sixtop_vars).handler = handler;
}

//======= scheduling

void sixtop_request(OpenMote* self, uint8_t code, open_addr_t* neighbor, uint8_t numCells){
    OpenQueueEntry_t* pkt;
    uint8_t           len;
    uint8_t           container;
    uint8_t           frameID;
    cellInfo_ht       cellList[SCHEDULEIEMAXNUMCELLS];
   
    memset(cellList,0,sizeof(cellList));
   
    // filter parameters
    if((self->sixtop_vars).six2six_state!=SIX_IDLE){
        return;
    }
    if (neighbor==NULL){
        return;
    }
   
    if ((self->sixtop_vars).handler == SIX_HANDLER_NONE) {
        // sxitop handler must not be NONE
        return;
    }
   
    // generate candidate cell list
    if (code == IANA_6TOP_CMD_ADD){
        if ( sixtop_candidateAddCellList(self, &frameID,cellList,numCells)==FALSE){
              return;
        } else{
            // container to be define by SF, currently equals to frameID
            container  = frameID;
        }
    }
    if (code == IANA_6TOP_CMD_DELETE){
        if ( sixtop_candidateRemoveCellList(self, &frameID,cellList,neighbor,numCells)==FALSE){
              return;
        } else{
            // container to be define by SF, currently equals to frameID
            container  = frameID;
        }
    }
    
    // get a free packet buffer
    pkt = openqueue_getFreePacketBuffer(self, COMPONENT_SIXTOP_RES);
    if (pkt==NULL) {
 openserial_printError(self, 
            COMPONENT_SIXTOP_RES,
            ERR_NO_FREE_PACKET_BUFFER,
            (errorparameter_t)0,
            (errorparameter_t)0
        );
        return;
    }
   
    // update state
    (self->sixtop_vars).six2six_state  = SIX_SENDING_REQUEST;
   
    // take ownership
    pkt->creator = COMPONENT_SIXTOP_RES;
    pkt->owner   = COMPONENT_SIXTOP_RES;
   
    memcpy(&(pkt->l2_nextORpreviousHop),neighbor,sizeof(open_addr_t));
   
    // create packet
    len  = 0;
    if (code == IANA_6TOP_CMD_ADD || IANA_6TOP_CMD_DELETE){
        len += processIE_prepend_sixCelllist(self, pkt,cellList);
        // reserve space for container
 packetfunctions_reserveHeaderSize(self, pkt,sizeof(uint8_t));
        // write header
        *((uint8_t*)(pkt->payload)) = container;
        len+=1;
        // reserve space for numCells
 packetfunctions_reserveHeaderSize(self, pkt,sizeof(uint8_t));
        // write header
        *((uint8_t*)(pkt->payload)) = numCells;
        len+=1;
    } else {
        // reserve space for container
 packetfunctions_reserveHeaderSize(self, pkt,sizeof(uint8_t));
        // write header
        *((uint8_t*)(pkt->payload)) = container;
        len+=1;
    }
    
    len += processIE_prepend_sixGeneralMessage(self, pkt,code);
    len += processIE_prepend_sixSubID(self, pkt);
 processIE_prepend_sixtopIE(self, pkt,len);
   
    // indicate IEs present
    pkt->l2_payloadIEpresent = TRUE;
   
    // send packet
 sixtop_send(self, pkt);
    
    //update states
    switch(code){
    case IANA_6TOP_CMD_ADD:
        (self->sixtop_vars).six2six_state = SIX_WAIT_ADDREQUEST_SENDDONE;
        break;
    case IANA_6TOP_CMD_DELETE:
        (self->sixtop_vars).six2six_state = SIX_WAIT_DELETEREQUEST_SENDDONE;
        break;
    case IANA_6TOP_CMD_COUNT:
        (self->sixtop_vars).six2six_state = SIX_WAIT_COUNTREQUEST_SENDDONE;
        break;
    case IANA_6TOP_CMD_LIST:
        (self->sixtop_vars).six2six_state = SIX_WAIT_LISTREQUEST_SENDDONE;
        break;
    case IANA_6TOP_CMD_CLEAR:
        (self->sixtop_vars).six2six_state = SIX_WAIT_CLEARREQUEST_SENDDONE;
        break;
    }
   
    // arm timeout
 opentimers_setPeriod(self, 
        (self->sixtop_vars).timeoutTimerId,
        TIME_MS,
        SIX2SIX_TIMEOUT_MS
    );
 opentimers_restart(self, (self->sixtop_vars).timeoutTimerId);
}

void sixtop_addORremoveCellByInfo(OpenMote* self, uint8_t code,open_addr_t* neighbor,cellInfo_ht* cellInfo){
    OpenQueueEntry_t* pkt;
    uint8_t           len;
    uint8_t           frameID;
    uint8_t           container;
    cellInfo_ht       cellList[SCHEDULEIEMAXNUMCELLS];
   
    memset(cellList,0,sizeof(cellList));
   
    // filter parameters
    if ((self->sixtop_vars).six2six_state!=SIX_IDLE){
        return;
    }
    if (neighbor==NULL){
        return;
    }
    if ((self->sixtop_vars).handler == SIX_HANDLER_NONE) {
        // sixtop handler must not be NONE
        return;
    }
   
    // set cell list (only first one is to be removed)
    frameID        = SCHEDULE_MINIMAL_6TISCH_DEFAULT_SLOTFRAME_HANDLE;
    container      = frameID;
    memcpy(&(cellList[0]),cellInfo,sizeof(cellList));
   
   
    // get a free packet buffer
    pkt = openqueue_getFreePacketBuffer(self, COMPONENT_SIXTOP_RES);
    if(pkt==NULL) {
 openserial_printError(self, 
            COMPONENT_SIXTOP_RES,
            ERR_NO_FREE_PACKET_BUFFER,
            (errorparameter_t)0,
            (errorparameter_t)0
        );
        return;
    }
   
    // update state
    (self->sixtop_vars).six2six_state = SIX_SENDING_REQUEST;
   
    // declare ownership over that packet
    pkt->creator = COMPONENT_SIXTOP_RES;
    pkt->owner   = COMPONENT_SIXTOP_RES;
      
    memcpy(&(pkt->l2_nextORpreviousHop),neighbor,sizeof(open_addr_t));
    
    len  = 0;
    len += processIE_prepend_sixCelllist(self, pkt,cellList);
    // reserve space for container
 packetfunctions_reserveHeaderSize(self, pkt,sizeof(uint8_t));
    // write header
    *((uint8_t*)(pkt->payload)) = container;
    len+=1;
    // reserve space for numCells
 packetfunctions_reserveHeaderSize(self, pkt,sizeof(uint8_t));
    // write header
    *((uint8_t*)(pkt->payload)) = 1;
    len+=1;
    
    len += processIE_prepend_sixGeneralMessage(self, pkt,code);
    len += processIE_prepend_sixSubID(self, pkt);
 processIE_prepend_sixtopIE(self, pkt,len);
   
    // indicate IEs present
    pkt->l2_payloadIEpresent = TRUE;
   
    // send packet
 sixtop_send(self, pkt);
   
    //update states
    switch(code){
    case IANA_6TOP_CMD_ADD:
        (self->sixtop_vars).six2six_state = SIX_WAIT_ADDREQUEST_SENDDONE;
        break;
    case IANA_6TOP_CMD_DELETE:
        (self->sixtop_vars).six2six_state = SIX_WAIT_DELETEREQUEST_SENDDONE;
        break;
    }
   
   // arm timeout
 opentimers_setPeriod(self, 
      (self->sixtop_vars).timeoutTimerId,
      TIME_MS,
      SIX2SIX_TIMEOUT_MS
   );
 opentimers_restart(self, (self->sixtop_vars).timeoutTimerId);
}

//======= maintaning 
void sixtop_maintaining(OpenMote* self, uint16_t slotOffset,open_addr_t* neighbor){
    slotinfo_element_t info;
    cellInfo_ht linkInfo;
 schedule_getSlotInfo(self, slotOffset,neighbor,&info);
    if(info.link_type != CELLTYPE_OFF){
        linkInfo.tsNum       = slotOffset;
        linkInfo.choffset    = info.channelOffset;
        linkInfo.linkoptions = info.link_type;
        (self->sixtop_vars).handler  = SIX_HANDLER_MAINTAIN;
 sixtop_addORremoveCellByInfo(self, IANA_6TOP_CMD_DELETE,neighbor, &linkInfo);
    } else {
        //should log this error
        
        return;
    }
}

//======= from upper layer

owerror_t sixtop_send(OpenMote* self, OpenQueueEntry_t *msg) {
   
   // set metadata
   msg->owner        = COMPONENT_SIXTOP;
   msg->l2_frameType = IEEE154_TYPE_DATA;


   // set l2-security attributes
   msg->l2_securityLevel   = IEEE802154_SECURITY_LEVEL;
   msg->l2_keyIdMode       = IEEE802154_SECURITY_KEYIDMODE; 
   msg->l2_keyIndex        = IEEE802154_SECURITY_K2_KEY_INDEX;

   if (msg->l2_payloadIEpresent == FALSE) {
      return sixtop_send_internal(self, 
         msg,
         FALSE
      );
   } else {
      return sixtop_send_internal(self, 
         msg,
         TRUE
      );
   }
}

//======= from lower layer

void task_sixtopNotifSendDone(OpenMote* self) {
   OpenQueueEntry_t* msg;
   
   // get recently-sent packet from openqueue
   msg = openqueue_sixtopGetSentPacket(self);
   if (msg==NULL) {
 openserial_printCritical(self, 
         COMPONENT_SIXTOP,
         ERR_NO_SENT_PACKET,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   // take ownership
   msg->owner = COMPONENT_SIXTOP;
   
   // update neighbor statistics
   if (msg->l2_sendDoneError==E_SUCCESS) {
 neighbors_indicateTx(self, 
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         TRUE,
         &msg->l2_asn
      );
   } else {
 neighbors_indicateTx(self, 
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         FALSE,
         &msg->l2_asn
      );
   }
   
   // send the packet to where it belongs
   switch (msg->creator) {
      
      case COMPONENT_SIXTOP:
         if (msg->l2_frameType==IEEE154_TYPE_BEACON) {
            // this is a EB
            
            // not busy sending EB anymore
            (self->sixtop_vars).busySendingEB = FALSE;
         } else {
            // this is a KA
            
            // not busy sending KA anymore
            (self->sixtop_vars).busySendingKA = FALSE;
         }
         // discard packets
 openqueue_freePacketBuffer(self, msg);
         
         // restart a random timer
         (self->sixtop_vars).periodMaintenance = 872+( openrandom_get16b(self)&0xff);
 opentimers_setPeriod(self, 
            (self->sixtop_vars).maintenanceTimerId,
            TIME_MS,
            (self->sixtop_vars).periodMaintenance
         );
         break;
      
      case COMPONENT_SIXTOP_RES:
 sixtop_six2six_sendDone(self, msg,msg->l2_sendDoneError);
         break;
      
      default:
         // send the rest up the stack
 iphc_sendDone(self, msg,msg->l2_sendDoneError);
         break;
   }
}

void task_sixtopNotifReceive(OpenMote* self) {
    OpenQueueEntry_t* msg;
    uint16_t          lenIE;
    // get received packet from openqueue
    msg = openqueue_sixtopGetReceivedPacket(self);
    if (msg==NULL) {
 openserial_printCritical(self, 
            COMPONENT_SIXTOP,
            ERR_NO_RECEIVED_PACKET,
            (errorparameter_t)0,
            (errorparameter_t)0
        );
        return;
    }
   
    // take ownership
    msg->owner = COMPONENT_SIXTOP;
   
    // process the header IEs
    lenIE=0;
    if(
        msg->l2_frameType              == IEEE154_TYPE_DATA  &&
        msg->l2_payloadIEpresent       == TRUE               &&
 sixtop_processIEs(self, msg, &lenIE) == FALSE
    ) {
        // free the packet's RAM memory
 openqueue_freePacketBuffer(self, msg);
        //log error
        return;
    }
   
    // toss the header IEs
 packetfunctions_tossHeader(self, msg,lenIE);
   
    // update neighbor statistics
 neighbors_indicateRx(self, 
        &(msg->l2_nextORpreviousHop),
        msg->l1_rssi,
        &msg->l2_asn,
        msg->l2_joinPriorityPresent,
        msg->l2_joinPriority
    );
   
    // reset it to avoid race conditions with this var.
    msg->l2_joinPriorityPresent = FALSE; 
   
    // send the packet up the stack, if it qualifies
    switch (msg->l2_frameType) {
    case IEEE154_TYPE_BEACON:
    case IEEE154_TYPE_DATA:
    case IEEE154_TYPE_CMD:
        if (msg->length>0) {
            // send to upper layer
 iphc_receive(self, msg);
        } else {
            // free up the RAM
 openqueue_freePacketBuffer(self, msg);
        }
            break;
    case IEEE154_TYPE_ACK:
    default:
        // free the packet's RAM memory
 openqueue_freePacketBuffer(self, msg);
        // log the error
 openserial_printError(self, 
            COMPONENT_SIXTOP,
            ERR_MSG_UNKNOWN_TYPE,
            (errorparameter_t)msg->l2_frameType,
            (errorparameter_t)0
        );
        break;
    }
}

//======= debugging

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_myDAGrank(OpenMote* self) {
   uint16_t output;
   
   output = 0;
   
   output = neighbors_getMyDAGrank(self);
 openserial_printStatus(self, STATUS_DAGRANK,(uint8_t*)&output,sizeof(uint16_t));
   return TRUE;
}

/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_kaPeriod(OpenMote* self) {
   uint16_t output;
   
   output = (self->sixtop_vars).kaPeriod;
   
 openserial_printStatus(self, 
       STATUS_KAPERIOD,
       (uint8_t*)&output,
       sizeof(output)
   );
   return TRUE;
}

void sixtop_setIsResponseEnabled(OpenMote* self, bool isEnabled){
    (self->sixtop_vars).isResponseEnabled = isEnabled;
}

//=========================== private =========================================

/**
\brief Transfer packet to MAC.

This function adds a IEEE802.15.4 header to the packet and leaves it the 
OpenQueue buffer. The very last thing it does is assigning this packet to the 
virtual component COMPONENT_SIXTOP_TO_IEEE802154E. Whenever it gets a change,
IEEE802154E will handle the packet.

\param[in] msg The packet to the transmitted
\param[in] iePresent Indicates wheter an Information Element is present in the
   packet.
\param[in] frameVersion The frame version to write in the packet.

\returns E_SUCCESS iff successful.
*/
owerror_t sixtop_send_internal(OpenMote* self, 
   OpenQueueEntry_t* msg, 
   bool    payloadIEPresent) {

   // assign a number of retries
   if (
 packetfunctions_isBroadcastMulticast(self, &(msg->l2_nextORpreviousHop))==TRUE
      ) {
      msg->l2_retriesLeft = 1;
   } else {
      msg->l2_retriesLeft = TXRETRIES + 1;
   }
   // record this packet's dsn (for matching the ACK)
   msg->l2_dsn = (self->sixtop_vars).dsn++;
   // this is a new packet which I never attempted to send
   msg->l2_numTxAttempts = 0;
   // transmit with the default TX power
   msg->l1_txPower = TX_POWER;
   // add a IEEE802.15.4 header
 ieee802154_prependHeader(self, msg,
                            msg->l2_frameType,
                            payloadIEPresent,
                            msg->l2_dsn,
                            &(msg->l2_nextORpreviousHop)
                            );
   // change owner to IEEE802154E fetches it from queue
   msg->owner  = COMPONENT_SIXTOP_TO_IEEE802154E;
   return E_SUCCESS;
}

// timer interrupt callbacks

void sixtop_maintenance_timer_cb(OpenMote* self, opentimer_id_t id) {
 scheduler_push_task(self, timer_sixtop_management_fired,TASKPRIO_SIXTOP);
}

void sixtop_timeout_timer_cb(OpenMote* self, opentimer_id_t id) {
 scheduler_push_task(self, timer_sixtop_six2six_timeout_fired,TASKPRIO_SIXTOP_TIMEOUT);
}

//======= EB/KA task

/**
\brief Timer handlers which triggers MAC management task.

This function is called in task context by the scheduler after the RES timer
has fired. This timer is set to fire every second, on average.

The body of this function executes one of the MAC management task.
*/
void timer_sixtop_management_fired(OpenMote* self) {
   scheduleEntry_t* entry;
   (self->sixtop_vars).mgtTaskCounter = ((self->sixtop_vars).mgtTaskCounter+1)%(self->sixtop_vars).ebPeriod;
   
   switch ((self->sixtop_vars).mgtTaskCounter) {
      case 0:
         // called every EBPERIOD seconds
 sixtop_sendEB(self);
         break;
      case 1:
         // called every EBPERIOD seconds
 neighbors_removeOld(self);
         break;
      case 2:
         // called every EBPERIOD seconds
         entry = schedule_statistic_poorLinkQuality(self);
         if (
             entry       != NULL                        && \
             entry->type != CELLTYPE_OFF                && \
             entry->type != CELLTYPE_TXRX               
         ){
 sixtop_maintaining(self, entry->slotOffset,&(entry->neighbor));
         }
      default:
         // called every second, except third times every EBPERIOD seconds
 sixtop_sendKA(self);
         break;
   }
}

/**
\brief Send an EB.

This is one of the MAC management tasks. This function inlines in the
timers_res_fired() function, but is declared as a separate function for better
readability of the code.
*/
port_INLINE void sixtop_sendEB(OpenMote* self) {
   OpenQueueEntry_t* eb;
   uint8_t len;
   
   len = 0;
   
   if (( ieee154e_isSynch(self)==FALSE) || ( neighbors_getMyDAGrank(self)==DEFAULTDAGRANK)){
      // I'm not sync'ed or I did not acquire a DAGrank
      
      // delete packets genereted by this module (EB and KA) from openqueue
 openqueue_removeAllCreatedBy(self, COMPONENT_SIXTOP);
      
      // I'm now busy sending an EB
      (self->sixtop_vars).busySendingEB = FALSE;
      
      // stop here
      return;
   }
   
   if ((self->sixtop_vars).busySendingEB==TRUE) {
      // don't continue if I'm still sending a previous EB
      return;
   }
   
   // if I get here, I will send an EB
   
   // get a free packet buffer
   eb = openqueue_getFreePacketBuffer(self, COMPONENT_SIXTOP);
   if (eb==NULL) {
 openserial_printError(self, COMPONENT_SIXTOP,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      return;
   }
   
   // declare ownership over that packet
   eb->creator = COMPONENT_SIXTOP;
   eb->owner   = COMPONENT_SIXTOP;
   
   // reserve space for EB-specific header
   // reserving for IEs.
   len += processIE_prependSlotframeLinkIE(self, eb);
   len += processIE_prependChannelHoppingIE(self, eb);
   len += processIE_prependTSCHTimeslotIE(self, eb);
   len += processIE_prependSyncIE(self, eb);
   
   //add IE header 
 processIE_prependMLMEIE(self, eb,len);
  
   // some l2 information about this packet
   eb->l2_frameType                     = IEEE154_TYPE_BEACON;
   eb->l2_nextORpreviousHop.type        = ADDR_16B;
   eb->l2_nextORpreviousHop.addr_16b[0] = 0xff;
   eb->l2_nextORpreviousHop.addr_16b[1] = 0xff;
   
   //I has an IE in my payload
   eb->l2_payloadIEpresent = TRUE;

   // set l2-security attributes
   eb->l2_securityLevel   = IEEE802154_SECURITY_LEVEL_BEACON;
   eb->l2_keyIdMode       = IEEE802154_SECURITY_KEYIDMODE;
   eb->l2_keyIndex        = IEEE802154_SECURITY_K1_KEY_INDEX;

   // put in queue for MAC to handle
 sixtop_send_internal(self, eb,eb->l2_payloadIEpresent);
   
   // I'm now busy sending an EB
   (self->sixtop_vars).busySendingEB = TRUE;
}

/**
\brief Send an keep-alive message, if necessary.

This is one of the MAC management tasks. This function inlines in the
timers_res_fired() function, but is declared as a separate function for better
readability of the code.
*/
port_INLINE void sixtop_sendKA(OpenMote* self) {
   OpenQueueEntry_t* kaPkt;
   open_addr_t*      kaNeighAddr;
   
   if ( ieee154e_isSynch(self)==FALSE) {
      // I'm not sync'ed
      
      // delete packets genereted by this module (EB and KA) from openqueue
 openqueue_removeAllCreatedBy(self, COMPONENT_SIXTOP);
      
      // I'm now busy sending a KA
      (self->sixtop_vars).busySendingKA = FALSE;
      
      // stop here
      return;
   }
   
   if ((self->sixtop_vars).busySendingKA==TRUE) {
      // don't proceed if I'm still sending a KA
      return;
   }
   
   kaNeighAddr = neighbors_getKANeighbor(self, (self->sixtop_vars).kaPeriod);
   if (kaNeighAddr==NULL) {
      // don't proceed if I have no neighbor I need to send a KA to
      return;
   }
   
   // if I get here, I will send a KA
   
   // get a free packet buffer
   kaPkt = openqueue_getFreePacketBuffer(self, COMPONENT_SIXTOP);
   if (kaPkt==NULL) {
 openserial_printError(self, COMPONENT_SIXTOP,ERR_NO_FREE_PACKET_BUFFER,
                            (errorparameter_t)1,
                            (errorparameter_t)0);
      return;
   }
   
   // declare ownership over that packet
   kaPkt->creator = COMPONENT_SIXTOP;
   kaPkt->owner   = COMPONENT_SIXTOP;
   
   // some l2 information about this packet
   kaPkt->l2_frameType = IEEE154_TYPE_DATA;
   memcpy(&(kaPkt->l2_nextORpreviousHop),kaNeighAddr,sizeof(open_addr_t));
   
   // set l2-security attributes
   kaPkt->l2_securityLevel   = IEEE802154_SECURITY_LEVEL;
   kaPkt->l2_keyIdMode       = IEEE802154_SECURITY_KEYIDMODE;
   kaPkt->l2_keyIndex        = IEEE802154_SECURITY_K2_KEY_INDEX;

   // put in queue for MAC to handle
 sixtop_send_internal(self, kaPkt,FALSE);
   
   // I'm now busy sending a KA
   (self->sixtop_vars).busySendingKA = TRUE;

#ifdef OPENSIM
 debugpins_ka_set(self);
 debugpins_ka_clr(self);
#endif
}

//======= six2six task

void timer_sixtop_six2six_timeout_fired(OpenMote* self) {
   // timeout timer fired, reset the state of sixtop to idle
   (self->sixtop_vars).six2six_state = SIX_IDLE;
   (self->sixtop_vars).handler = SIX_HANDLER_NONE;
 opentimers_stop(self, (self->sixtop_vars).timeoutTimerId);
}

void sixtop_six2six_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error){
   uint8_t i,numOfCells;
   uint8_t* ptr;
   cellInfo_ht cellList[SCHEDULEIEMAXNUMCELLS];
   
   memset(cellList,0,SCHEDULEIEMAXNUMCELLS*sizeof(cellInfo_ht));
  
   ptr = msg->l2_sixtop_cellObjects;
   numOfCells = msg->l2_sixtop_numOfCells;
   msg->owner = COMPONENT_SIXTOP_RES;
  
   if(error == E_FAIL) {
      (self->sixtop_vars).six2six_state = SIX_IDLE;
 openqueue_freePacketBuffer(self, msg);
      return;
   }

    switch ((self->sixtop_vars).six2six_state) {
    case SIX_WAIT_ADDREQUEST_SENDDONE:
        (self->sixtop_vars).six2six_state = SIX_WAIT_ADDRESPONSE;
        break;
    case SIX_WAIT_DELETEREQUEST_SENDDONE:
        (self->sixtop_vars).six2six_state = SIX_WAIT_DELETERESPONSE;
        break;
    case SIX_WAIT_LISTREQUEST_SENDDONE:
        (self->sixtop_vars).six2six_state = SIX_WAIT_LISTRESPONSE;
        break;
    case SIX_WAIT_COUNTREQUEST_SENDDONE:
        (self->sixtop_vars).six2six_state = SIX_WAIT_COUNTRESPONSE;
        break;
    case SIX_WAIT_CLEARREQUEST_SENDDONE:
        (self->sixtop_vars).six2six_state = SIX_WAIT_CLEARRESPONSE;
        break;
    case SIX_WAIT_RESPONSE_SENDDONE:
        if (msg->l2_sixtop_returnCode == IANA_6TOP_RC_SUCCESS && error == E_SUCCESS){
            if (
                msg->l2_sixtop_requestCommand == IANA_6TOP_CMD_ADD ||
                msg->l2_sixtop_requestCommand == IANA_6TOP_CMD_DELETE
            ){
                if (numOfCells>0){
                    for (i=0;i<numOfCells;i++){
                        //TimeSlot 2B
                        cellList[i].tsNum       = *(ptr);
                        cellList[i].tsNum      |= (*(ptr+1))<<8;
                        //Ch.Offset 2B
                        cellList[i].choffset    = *(ptr+2);
                        cellList[i].choffset   |= (*(ptr+3))<<8;
                        ptr += 4;
                        // mark with linkoptions as ocuppied
                        cellList[i].linkoptions = CELLTYPE_TX;
                    }
                    if (msg->l2_sixtop_requestCommand == IANA_6TOP_CMD_ADD){
 sixtop_addCellsByState(self, 
                              msg->l2_sixtop_frameID,
                              cellList,
                              &(msg->l2_nextORpreviousHop),
                              (self->sixtop_vars).six2six_state);
                    } else {
 sixtop_removeCellsByState(self, 
                              msg->l2_sixtop_frameID,
                              cellList,
                              &(msg->l2_nextORpreviousHop));
                    }
                }
                  
            } else{
                if (msg->l2_sixtop_requestCommand == IANA_6TOP_CMD_CLEAR){
 schedule_removeAllCells(self, msg->l2_sixtop_frameID,
                                          &(msg->l2_nextORpreviousHop));
                }
            }
        }
        
        (self->sixtop_vars).six2six_state = SIX_IDLE;
        (self->sixtop_vars).handler = SIX_HANDLER_NONE;
 opentimers_stop(self, (self->sixtop_vars).timeoutTimerId);
       
        if ((self->sixtop_vars).handler == SIX_HANDLER_MAINTAIN){
 sixtop_request(self, 
                IANA_6TOP_CMD_DELETE,
                &(msg->l2_nextORpreviousHop),
                1
            );
 sixtop_request(self, IANA_6TOP_CMD_ADD,&(msg->l2_nextORpreviousHop),1);
            (self->sixtop_vars).handler = SIX_HANDLER_NONE;
        }
        break;
    default:
        //log error
        (self->sixtop_vars).six2six_state = SIX_IDLE;
        break;
    }
  
    // discard reservation packets this component has created
 openqueue_freePacketBuffer(self, msg);
}

port_INLINE bool sixtop_processIEs(OpenMote* self, OpenQueueEntry_t* pkt, uint16_t * lenIE) {
    uint8_t ptr;
    uint8_t temp_8b,gr_elem_id,subid;
    uint16_t temp_16b,len,sublen;
    uint8_t version,commandIdORcode;
    uint8_t sfId;
   
    ptr=0;  
  
    //candidate IE header  if type ==0 header IE if type==1 payload IE
    temp_8b = *((uint8_t*)(pkt->payload)+ptr);
    ptr++;
    temp_16b = temp_8b + ((*((uint8_t*)(pkt->payload)+ptr))<<8);
    ptr++;
    *lenIE = ptr;
    if(
        (temp_16b & IEEE802154E_DESC_TYPE_PAYLOAD_IE) == 
        IEEE802154E_DESC_TYPE_PAYLOAD_IE
    ){
        //payload IE - last bit is 1
        len = temp_16b & IEEE802154E_DESC_LEN_PAYLOAD_IE_MASK;
        gr_elem_id = 
            (temp_16b & IEEE802154E_DESC_GROUPID_PAYLOAD_IE_MASK)>>
            IEEE802154E_DESC_GROUPID_PAYLOAD_IE_SHIFT;
    }else {
        //header IE - last bit is 0
        len = temp_16b & IEEE802154E_DESC_LEN_HEADER_IE_MASK;
        gr_elem_id = (temp_16b & IEEE802154E_DESC_ELEMENTID_HEADER_IE_MASK)>>
            IEEE802154E_DESC_ELEMENTID_HEADER_IE_SHIFT; 
    }
  
    *lenIE += len;
    //now determine sub elements if any
    switch(gr_elem_id){
    //this is the only groupID that we parse. See page 82.  
    case IEEE802154E_MLME_IE_GROUPID:
        //IE content can be any of the sub-IEs. Parse and see which
        do{
            //read sub IE header
            temp_8b = *((uint8_t*)(pkt->payload)+ptr);
            ptr = ptr + 1;
            temp_16b = temp_8b + ((*((uint8_t*)(pkt->payload)+ptr))<<8);
            ptr = ptr + 1;
            len = len - 2; //remove header fields len
            if(
                (temp_16b & IEEE802154E_DESC_TYPE_LONG) == 
                IEEE802154E_DESC_TYPE_LONG
            ){
                //long sub-IE - last bit is 1
                sublen = temp_16b & IEEE802154E_DESC_LEN_LONG_MLME_IE_MASK;
                subid= 
                    (temp_16b & IEEE802154E_DESC_SUBID_LONG_MLME_IE_MASK)>>
                    IEEE802154E_DESC_SUBID_LONG_MLME_IE_SHIFT; 
            } else {
                //short IE - last bit is 0
                sublen = temp_16b & IEEE802154E_DESC_LEN_SHORT_MLME_IE_MASK;
                subid = (temp_16b & IEEE802154E_DESC_SUBID_SHORT_MLME_IE_MASK)>>
                IEEE802154E_DESC_SUBID_SHORT_MLME_IE_SHIFT; 
            }
            switch(subid){
            case IANA_6TOP_SUBIE_ID:
                temp_8b = *((uint8_t*)(pkt->payload)+ptr);
                ptr = ptr + 1;
                // get 6P version and command ID
                version   = temp_8b & 0x0f;
                commandIdORcode = (temp_8b >> 4) & 0x0f;
                // get sf_id
                sfId = *((uint8_t*)(pkt->payload)+ptr);
                ptr = ptr + 1;
 sixtop_notifyReceiveCommand(self, version,commandIdORcode,sfId,ptr,sublen-2,pkt);
                ptr += sublen-2;
                break;
            default:
                return FALSE;
            }
            len = len - sublen;
        } while(len>0);
        break;
    case SIXTOP_IE_GROUPID:
        subid = *((uint8_t*)(pkt->payload)+ptr);
        ptr += 1;
        sublen = len - 1;
        switch(subid){
            case IANA_6TOP_SUBIE_ID:
                temp_8b = *((uint8_t*)(pkt->payload)+ptr);
                ptr = ptr + 1;
                // get 6P version and command ID
                version   = temp_8b & 0x0f;
                commandIdORcode = (temp_8b >> 4) & 0x0f;
                // get sf_id
                sfId = *((uint8_t*)(pkt->payload)+ptr);
                ptr = ptr + 1;
 sixtop_notifyReceiveCommand(self, version,commandIdORcode,sfId,ptr,sublen-2,pkt);
                ptr += sublen-2;
                break;
            default:
                return FALSE;
        }
        break;
    default:
        *lenIE = 0;//no header or not recognized.
        return FALSE;
    }
    if (*lenIE>127) {
        // log the error
 openserial_printError(self, COMPONENT_IEEE802154E,ERR_HEADER_TOO_LONG,
                           (errorparameter_t)*lenIE,
                           (errorparameter_t)1);
    }
    return TRUE;
}

void sixtop_notifyReceiveCommand(OpenMote* self, 
    uint8_t           version, 
    uint8_t           commandIdORcode, 
    uint8_t           sfId,
    uint8_t           ptr,
    uint8_t           length,
    OpenQueueEntry_t* pkt
){
    uint8_t           code;
    uint8_t           frameID;
    uint8_t           numOfCells;
    uint16_t          count;
    cellInfo_ht       cellList[SCHEDULEIEMAXNUMCELLS];
    OpenQueueEntry_t* response_pkt;
    uint8_t           len = 0;
    uint8_t           container;
    
    memset(cellList,0,sizeof(cellList));
    
    // get a free packet buffer
    response_pkt = openqueue_getFreePacketBuffer(self, COMPONENT_SIXTOP_RES);
    if (response_pkt==NULL) {
 openserial_printError(self, 
            COMPONENT_SIXTOP_RES,
            ERR_NO_FREE_PACKET_BUFFER,
            (errorparameter_t)0,
            (errorparameter_t)0
        );
        return;
    }
   
    // take ownership
    response_pkt->creator = COMPONENT_SIXTOP_RES;
    response_pkt->owner   = COMPONENT_SIXTOP_RES;
    
    memcpy(&(response_pkt->l2_nextORpreviousHop),
           &(pkt->l2_nextORpreviousHop),
           sizeof(open_addr_t)
    );
    
    //===== if the version or sfID are correct
    if (version != IANA_6TOP_6P_VERSION || sfId != SFID_SF0){
        if (version != IANA_6TOP_6P_VERSION){
            code = IANA_6TOP_RC_VER_ERR;
        } else {
            code = IANA_6TOP_RC_SFID_ERR;
        }
    } else {
        //====== the version and sfID are correct
        //------ if this is a command
        if (
            commandIdORcode == IANA_6TOP_CMD_ADD    ||
            commandIdORcode == IANA_6TOP_CMD_DELETE ||
            commandIdORcode == IANA_6TOP_CMD_COUNT  ||
            commandIdORcode == IANA_6TOP_CMD_LIST   ||
            commandIdORcode == IANA_6TOP_CMD_CLEAR
        ){
            // if I am already in a 6top transactions
            if ((self->sixtop_vars).six2six_state != SIX_IDLE){
                code = IANA_6TOP_RC_ERR;
            } else {
                (self->sixtop_vars).six2six_state = SIX_REQUEST_RECEIVED;

                switch(commandIdORcode){
                case IANA_6TOP_CMD_ADD: 
                case IANA_6TOP_CMD_DELETE:
                    numOfCells = *((uint8_t*)(pkt->payload)+ptr);
                    container  = *((uint8_t*)(pkt->payload)+ptr+1);
                    frameID = container;
 processIE_retrieve_sixCelllist(self, pkt,ptr+2,length-2,cellList);
                    if (
                        (
                          commandIdORcode == IANA_6TOP_CMD_ADD &&
 sixtop_areAvailableCellsToBeScheduled(self, frameID,numOfCells,cellList)
                        ) ||
                       (
                          commandIdORcode == IANA_6TOP_CMD_DELETE &&
 sixtop_areAvailableCellsToBeRemoved(self, frameID,numOfCells,cellList,&(pkt->l2_nextORpreviousHop))
                       )
                    ){
                        code = IANA_6TOP_RC_SUCCESS;
                        len += processIE_prepend_sixCelllist(self, response_pkt,cellList);
                    } else {
                        code = IANA_6TOP_RC_RESET;
                    }
                    break;
                case IANA_6TOP_CMD_COUNT:
                    container  = *((uint8_t*)(pkt->payload)+ptr);
                    frameID = container;
                    count = schedule_getCellsCounts(self, frameID,
                                                    CELLTYPE_RX,
                                                    &(pkt->l2_nextORpreviousHop));
                    code = IANA_6TOP_RC_SUCCESS;
 packetfunctions_reserveHeaderSize(self, response_pkt,2);
                    response_pkt->payload[0] = count      & 0xFF;
                    response_pkt->payload[1] = (count>>8) & 0xFF;
                    len = 2;
                    break;
                case IANA_6TOP_CMD_LIST:
                    container  = *((uint8_t*)(pkt->payload)+ptr);
                    frameID = container;
                    numOfCells = sixtop_getCelllist(self, frameID,
                                         &(pkt->l2_nextORpreviousHop),
                                         cellList);
                    code = IANA_6TOP_RC_SUCCESS;
                    if (numOfCells>0){
                        len += processIE_prepend_sixCelllist(self, response_pkt,cellList);
                    }
                    break;
                case IANA_6TOP_CMD_CLEAR:
                    container  = *((uint8_t*)(pkt->payload)+ptr);
                    frameID = container;
 schedule_removeAllCells(self, frameID,
                                            &(pkt->l2_nextORpreviousHop));
                    code = IANA_6TOP_RC_SUCCESS;
                    break;
                }
            }
            response_pkt->l2_sixtop_requestCommand = commandIdORcode;
            response_pkt->l2_sixtop_frameID        = frameID;
            
            len += processIE_prepend_sixGeneralMessage(self, response_pkt,code);
            len += processIE_prepend_sixSubID(self, response_pkt);
 processIE_prepend_sixtopIE(self, response_pkt,len);
            // indicate IEs present
            response_pkt->l2_payloadIEpresent = TRUE;
            if ((self->sixtop_vars).isResponseEnabled){
                // send packet
 sixtop_send(self, response_pkt);
            }
            // update state
            (self->sixtop_vars).six2six_state = SIX_WAIT_RESPONSE_SENDDONE;
            // arm timeout
 opentimers_setPeriod(self, 
                (self->sixtop_vars).timeoutTimerId,
                TIME_MS,
                SIX2SIX_TIMEOUT_MS
            );
 opentimers_restart(self, (self->sixtop_vars).timeoutTimerId);
        } else {
            //------ if this is a return code
            // if the code is SUCCESS
            if (commandIdORcode==IANA_6TOP_RC_SUCCESS){
                switch((self->sixtop_vars).six2six_state){
                case SIX_WAIT_ADDRESPONSE:
                case SIX_WAIT_DELETERESPONSE:
 processIE_retrieve_sixCelllist(self, pkt,ptr,length,cellList);
                    // always default frameID
                    frameID = SCHEDULE_MINIMAL_6TISCH_DEFAULT_SLOTFRAME_HANDLE;
                    if ((self->sixtop_vars).six2six_state == SIX_WAIT_ADDRESPONSE){
 sixtop_addCellsByState(self, frameID,
                                              cellList,
                                              &(pkt->l2_nextORpreviousHop),
                                              (self->sixtop_vars).six2six_state
                        );
                    } else {
 sixtop_removeCellsByState(self, 
                              frameID,
                              cellList,
                              &(pkt->l2_nextORpreviousHop));
                    }
                    break;
                case SIX_WAIT_COUNTRESPONSE:
                    count  = *((uint8_t*)(pkt->payload)+ptr);
                    ptr += 1;
                    count |= (*((uint8_t*)(pkt->payload)+ptr))<<8;
#ifdef GOLDEN_IMAGE_ROOT
 openserial_printInfo(self, COMPONENT_SIXTOP,ERR_SIXTOP_COUNT,
                           (errorparameter_t)count,
                           (errorparameter_t)(self->sixtop_vars).six2six_state);
#endif
                    break;
                case SIX_WAIT_LISTRESPONSE:
 processIE_retrieve_sixCelllist(self, pkt,ptr,length,cellList);
#ifdef GOLDEN_IMAGE_ROOT
                // print out first two cells in the list
 openserial_printInfo(self, COMPONENT_SIXTOP,ERR_SIXTOP_LIST,
                           (errorparameter_t)cellList[0].tsNum,
                           (errorparameter_t)cellList[1].tsNum);
#endif
                    break;
                case SIX_WAIT_CLEARRESPONSE:
                  
                    break;
                default:
                    code = IANA_6TOP_RC_ERR;
                }
            } else {
                // TBD...
            }
#ifdef GOLDEN_IMAGE_ROOT
 openserial_printInfo(self, COMPONENT_SIXTOP,ERR_SIXTOP_RETURNCODE,
                           (errorparameter_t)commandIdORcode,
                           (errorparameter_t)(self->sixtop_vars).six2six_state);
#endif
            (self->sixtop_vars).six2six_state = SIX_IDLE;
            (self->sixtop_vars).handler = SIX_HANDLER_NONE;
 opentimers_stop(self, (self->sixtop_vars).timeoutTimerId);
        }
    }
}

uint8_t sixtop_getCelllist(OpenMote* self, 
    uint8_t              frameID,
    open_addr_t*         neighbor,
    cellInfo_ht*         cellList
    ){
    uint8_t          i=0;
    scheduleEntry_t* scheduleWalker;
    scheduleEntry_t* currentEntry;
   
    INTERRUPT_DECLARATION();
    DISABLE_INTERRUPTS();
    
    if (frameID != SCHEDULE_MINIMAL_6TISCH_DEFAULT_SLOTFRAME_HANDLE){
        ENABLE_INTERRUPTS();
        return 0;
    }
    
    memset(cellList,0,SCHEDULEIEMAXNUMCELLS*sizeof(cellInfo_ht));
   
    scheduleWalker = schedule_getCurrentScheduleEntry(self);
    currentEntry   = scheduleWalker;
    do {
       if( packetfunctions_sameAddress(self, &(scheduleWalker->neighbor),neighbor)){
           cellList[i].tsNum        = scheduleWalker->slotOffset;
           cellList[i].choffset     = scheduleWalker->channelOffset;
           cellList[i].linkoptions  = scheduleWalker->type;
           i++;
       }
       scheduleWalker = scheduleWalker->next;
    }while(scheduleWalker!=currentEntry && i!=SCHEDULEIEMAXNUMCELLS);
   
    ENABLE_INTERRUPTS();
    return i;
}

//======= helper functions

bool sixtop_candidateAddCellList(OpenMote* self, 
      uint8_t*     frameID,
      cellInfo_ht* cellList,
      uint8_t      requiredCells
   ){
   frameLength_t i;
   uint8_t counter;
   uint8_t numCandCells;
   
   *frameID = schedule_getFrameHandle(self);
   
   numCandCells=0;
   for(counter=0;counter<SCHEDULEIEMAXNUMCELLS;counter++){
      i = openrandom_get16b(self)% schedule_getFrameLength(self);
      if( schedule_isSlotOffsetAvailable(self, i)==TRUE){
         cellList[numCandCells].tsNum       = i;
         cellList[numCandCells].choffset    = 0;
         cellList[numCandCells].linkoptions = CELLTYPE_TX;
         numCandCells++;
      }
   }
   
   if (numCandCells<requiredCells) {
      return FALSE;
   } else {
      return TRUE;
   }
}

bool sixtop_candidateRemoveCellList(OpenMote* self, 
      uint8_t*     frameID,
      cellInfo_ht* cellList,
      open_addr_t* neighbor,
      uint8_t      requiredCells
   ){
   uint8_t              i;
   uint8_t              numCandCells;
   slotinfo_element_t   info;
   
   *frameID        = schedule_getFrameHandle(self);
  
   numCandCells    = 0;
   for(i=0;i< schedule_getFrameLength(self);i++){
 schedule_getSlotInfo(self, i,neighbor,&info);
      if(info.link_type == CELLTYPE_TX){
         cellList[numCandCells].tsNum       = i;
         cellList[numCandCells].choffset    = info.channelOffset;
         cellList[numCandCells].linkoptions = CELLTYPE_TX;
         numCandCells++;
         if (numCandCells==SCHEDULEIEMAXNUMCELLS){
            break; // only delete one cell
         }
      }
   }
   
   if(numCandCells<requiredCells){
      return FALSE;
   }else{
      return TRUE;
   }
}

void sixtop_addCellsByState(OpenMote* self, 
      uint8_t      slotframeID,
      cellInfo_ht* cellList,
      open_addr_t* previousHop,
      uint8_t      state
   ){
   uint8_t     i;
   open_addr_t temp_neighbor;
  
   //set schedule according links
   
   for(i = 0;i<SCHEDULEIEMAXNUMCELLS;i++){
      //only schedule when the request side wants to schedule a tx cell
      if(cellList[i].linkoptions != CELLTYPE_OFF){
         switch(state) {
            case SIX_WAIT_RESPONSE_SENDDONE:
               memcpy(&temp_neighbor,previousHop,sizeof(open_addr_t));
               //add a RX link
 schedule_addActiveSlot(self, 
                  cellList[i].tsNum,
                  CELLTYPE_RX,
                  FALSE,
                  cellList[i].choffset,
                  &temp_neighbor
               );
               
               break;
            case SIX_WAIT_ADDRESPONSE:
               memcpy(&temp_neighbor,previousHop,sizeof(open_addr_t));
               //add a TX link
 schedule_addActiveSlot(self, 
                  cellList[i].tsNum,
                  CELLTYPE_TX,
                  FALSE,
                  cellList[i].choffset,
                  &temp_neighbor
               );
               break;
            default:
               //log error
               break;
         }
      }
   }
}

void sixtop_removeCellsByState(OpenMote* self, 
      uint8_t      slotframeID,
      cellInfo_ht* cellList,
      open_addr_t* previousHop
   ){
   uint8_t i;
   
   for(i=0;i<SCHEDULEIEMAXNUMCELLS;i++){   
      if(cellList[i].linkoptions != CELLTYPE_OFF){
 schedule_removeActiveSlot(self, 
            cellList[i].tsNum,
            previousHop
         );
      }
   }
}

bool sixtop_areAvailableCellsToBeScheduled(OpenMote* self, 
      uint8_t      frameID, 
      uint8_t      numOfCells, 
      cellInfo_ht* cellList
   ){
   uint8_t i;
   uint8_t bw;
   bool    available;
   
   i          = 0;
   bw         = numOfCells;
   available  = FALSE;
  
   if(bw == 0 || bw>SCHEDULEIEMAXNUMCELLS){
      // log wrong parameter error TODO
    
      available = FALSE;
   } else {
      do {
         if( schedule_isSlotOffsetAvailable(self, cellList[i].tsNum) == TRUE){
            bw--;
         } else {
            cellList[i].linkoptions = CELLTYPE_OFF;
         }
         i++;
      }while(i<SCHEDULEIEMAXNUMCELLS && bw>0);
      
      if(bw==0){
         //the rest link will not be scheduled, mark them as off type
         while(i<SCHEDULEIEMAXNUMCELLS){
            cellList[i].linkoptions = CELLTYPE_OFF;
            i++;
         }
         // local schedule can statisfy the bandwidth of cell request. 
         available = TRUE;
      } else {
         // local schedule can't statisfy the bandwidth of cell request
         available = FALSE;
      }
   }
   
   return available;
}

bool sixtop_areAvailableCellsToBeRemoved(OpenMote* self,       
        uint8_t      frameID, 
        uint8_t      numOfCells, 
        cellInfo_ht* cellList,
        open_addr_t* neighbor
    ){
   uint8_t              i;
   uint8_t              bw;
   bool                 available;
   slotinfo_element_t   info;
   
   i          = 0;
   bw         = numOfCells;
   available  = FALSE;
  
   if(bw == 0 || bw>SCHEDULEIEMAXNUMCELLS){
      // log wrong parameter error TODO
      available = FALSE;
   } else {
      do {
 schedule_getSlotInfo(self, cellList[i].tsNum,neighbor,&info);
          if(info.link_type == CELLTYPE_RX){
              bw--;
          } else {
              cellList[i].linkoptions = CELLTYPE_OFF;
          }
          i++;
        }while(i<SCHEDULEIEMAXNUMCELLS && bw>0);
      
        if(bw==0){
            //the rest link will not be scheduled, mark them as off type
            while(i<SCHEDULEIEMAXNUMCELLS){
                cellList[i].linkoptions = CELLTYPE_OFF;
                i++;
            }
            // local schedule can statisfy the bandwidth of cell request. 
            available = TRUE;
        } else {
            // local schedule can't statisfy the bandwidth of cell request
            available = FALSE;
        }
   }
   
   return available;
}
