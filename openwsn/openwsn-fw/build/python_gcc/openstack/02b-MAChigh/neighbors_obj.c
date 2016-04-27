/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2016-03-03 12:09:30.387882.
*/
#include "opendefs_obj.h"
#include "neighbors_obj.h"
#include "openqueue_obj.h"
#include "packetfunctions_obj.h"
#include "idmanager_obj.h"
#include "openserial_obj.h"
#include "IEEE802154E_obj.h"

//=========================== variables =======================================

// declaration of global variable _neighbors_vars_ removed during objectification.

//=========================== prototypes ======================================

void registerNewNeighbor(OpenMote* self, 
        open_addr_t* neighborID,
        int8_t       rssi,
        asn_t*       asnTimestamp,
        bool         joinPrioPresent,
        uint8_t      joinPrio
     );
bool isNeighbor(OpenMote* self, open_addr_t* neighbor);
void removeNeighbor(OpenMote* self, uint8_t neighborIndex);
bool isThisRowMatching(OpenMote* self, 
        open_addr_t* address,
        uint8_t      rowNumber
     );

//=========================== public ==========================================

/**
\brief Initializes this module.
*/
void neighbors_init(OpenMote* self) {
   
   // clear module variables
   memset(&(self->neighbors_vars),0,sizeof(neighbors_vars_t));
   
   // set myDAGrank
   if ( idmanager_getIsDAGroot(self)==TRUE) {
      (self->neighbors_vars).myDAGrank=MINHOPRANKINCREASE;
   } else {
      (self->neighbors_vars).myDAGrank=DEFAULTDAGRANK;
   }
}

//===== getters

/**
\brief Retrieve this mote's current DAG rank.

\returns This mote's current DAG rank.
*/
dagrank_t neighbors_getMyDAGrank(OpenMote* self) {
   return (self->neighbors_vars).myDAGrank;
}

/**
\brief Retrieve the number of neighbors this mote's currently knows of.

\returns The number of neighbors this mote's currently knows of.
*/
uint8_t neighbors_getNumNeighbors(OpenMote* self) {
   uint8_t i;
   uint8_t returnVal;
   
   returnVal=0;
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ((self->neighbors_vars).neighbors[i].used==TRUE) {
         returnVal++;
      }
   }
   return returnVal;
}

/**
\brief Retrieve my preferred parent's EUI64 address.

\param[out] addressToWrite Where to write the preferred parent's address to.
*/
bool neighbors_getPreferredParentEui64(OpenMote* self, open_addr_t* addressToWrite) {
   uint8_t   i;
   bool      foundPreferred;
   uint8_t   numNeighbors;
   dagrank_t minRankVal;
   uint8_t   minRankIdx;
   
   addressToWrite->type = ADDR_NONE;
   
   foundPreferred       = FALSE;
   numNeighbors         = 0;
   minRankVal           = MAXDAGRANK;
   minRankIdx           = MAXNUMNEIGHBORS+1;
   
   //===== step 1. Try to find preferred parent
   for (i=0; i<MAXNUMNEIGHBORS; i++) {
      if ((self->neighbors_vars).neighbors[i].used==TRUE){
         if ((self->neighbors_vars).neighbors[i].parentPreference==MAXPREFERENCE) {
            memcpy(addressToWrite,&((self->neighbors_vars).neighbors[i].addr_64b),sizeof(open_addr_t));
            addressToWrite->type=ADDR_64B;
            foundPreferred=TRUE;
         }
         // identify neighbor with lowest rank
         if ((self->neighbors_vars).neighbors[i].DAGrank < minRankVal) {
            minRankVal=(self->neighbors_vars).neighbors[i].DAGrank;
            minRankIdx=i;
         }
         numNeighbors++;
      }
   }
   
   //===== step 2. (backup) Promote neighbor with min rank to preferred parent
   if (foundPreferred==FALSE && numNeighbors > 0){
      // promote neighbor
      (self->neighbors_vars).neighbors[minRankIdx].parentPreference       = MAXPREFERENCE;
      (self->neighbors_vars).neighbors[minRankIdx].stableNeighbor         = TRUE;
      (self->neighbors_vars).neighbors[minRankIdx].switchStabilityCounter = 0;
      // return its address
      memcpy(addressToWrite,&((self->neighbors_vars).neighbors[minRankIdx].addr_64b),sizeof(open_addr_t));
      addressToWrite->type=ADDR_64B;
      foundPreferred=TRUE;         
   }
   
   return foundPreferred;
}

/**
\brief Find neighbor to which to send KA.

This function iterates through the neighbor table and identifies the neighbor
we need to send a KA to, if any. This neighbor satisfies the following
conditions:
- it is one of our preferred parents
- we haven't heard it for over kaPeriod

\param[in] kaPeriod The maximum number of slots I'm allowed not to have heard
   it.

\returns A pointer to the neighbor's address, or NULL if no KA is needed.
*/
open_addr_t* neighbors_getKANeighbor(OpenMote* self, uint16_t kaPeriod) {
   uint8_t         i;
   uint16_t        timeSinceHeard;
   open_addr_t*    addrPreferred;
   open_addr_t*    addrOther;
   
   // initialize
   addrPreferred = NULL;
   addrOther     = NULL;
   
   // scan through the neighbor table, and populate addrPreferred and addrOther
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ((self->neighbors_vars).neighbors[i].used==1) {
         timeSinceHeard = ieee154e_asnDiff(self, &(self->neighbors_vars).neighbors[i].asn);
         if (timeSinceHeard>kaPeriod) {
            // this neighbor needs to be KA'ed to
            if ((self->neighbors_vars).neighbors[i].parentPreference==MAXPREFERENCE) {
               // its a preferred parent
               addrPreferred = &((self->neighbors_vars).neighbors[i].addr_64b);
            } else {
               // its not a preferred parent
               // Note: commented out since policy is not to KA to non-preferred parents
               // addrOther =     &((self->neighbors_vars).neighbors[i].addr_64b);
            }
         }
      }
   }
   
   // return the EUI64 of the most urgent KA to send:
   // - if available, preferred parent
   // - if not, non-preferred parent
   if        (addrPreferred!=NULL) {
      return addrPreferred;
   } else if (addrOther!=NULL) {
      return addrOther;
   } else {
      return NULL;
   }
}

//===== interrogators

/**
\brief Indicate whether some neighbor is a stable neighbor

\param[in] address The address of the neighbor, a full 128-bit IPv6 addres.

\returns TRUE if that neighbor is stable, FALSE otherwise.
*/
bool neighbors_isStableNeighbor(OpenMote* self, open_addr_t* address) {
   uint8_t     i;
   open_addr_t temp_addr_64b;
   open_addr_t temp_prefix;
   bool        returnVal;
   
   // by default, not stable
   returnVal  = FALSE;
   
   // but neighbor's IPv6 address in prefix and EUI64
   switch (address->type) {
      case ADDR_128B:
 packetfunctions_ip128bToMac64b(self, address,&temp_prefix,&temp_addr_64b);
         break;
      default:
 openserial_printCritical(self, COMPONENT_NEIGHBORS,ERR_WRONG_ADDR_TYPE,
                               (errorparameter_t)address->type,
                               (errorparameter_t)0);
         return returnVal;
   }
   
   // iterate through neighbor table
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ( isThisRowMatching(self, &temp_addr_64b,i) && (self->neighbors_vars).neighbors[i].stableNeighbor==TRUE) {
         returnVal  = TRUE;
         break;
      }
   }
   
   return returnVal;
}

/**
\brief Indicate whether some neighbor is a preferred neighbor.

\param[in] address The EUI64 address of the neighbor.

\returns TRUE if that neighbor is preferred, FALSE otherwise.
*/
bool neighbors_isPreferredParent(OpenMote* self, open_addr_t* address) {
   uint8_t i;
   bool    returnVal;
   
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   // by default, not preferred
   returnVal = FALSE;
   
   // iterate through neighbor table
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ( isThisRowMatching(self, address,i) && (self->neighbors_vars).neighbors[i].parentPreference==MAXPREFERENCE) {
         returnVal  = TRUE;
         break;
      }
   }
   
   ENABLE_INTERRUPTS();
   return returnVal;
}

/**
\brief Indicate whether some neighbor has a lower DAG rank that me.

\param[in] index The index of that neighbor in the neighbor table.

\returns TRUE if that neighbor has a lower DAG rank than me, FALSE otherwise.
*/
bool neighbors_isNeighborWithLowerDAGrank(OpenMote* self, uint8_t index) {
   bool    returnVal;
   
   if ((self->neighbors_vars).neighbors[index].used==TRUE &&
       (self->neighbors_vars).neighbors[index].DAGrank < neighbors_getMyDAGrank(self)) { 
      returnVal = TRUE;
   } else {
      returnVal = FALSE;
   }
   
   return returnVal;
}


/**
\brief Indicate whether some neighbor has a lower DAG rank that me.

\param[in] index The index of that neighbor in the neighbor table.

\returns TRUE if that neighbor has a lower DAG rank than me, FALSE otherwise.
*/
bool neighbors_isNeighborWithHigherDAGrank(OpenMote* self, uint8_t index) {
   bool    returnVal;
   
   if ((self->neighbors_vars).neighbors[index].used==TRUE &&
       (self->neighbors_vars).neighbors[index].DAGrank >= neighbors_getMyDAGrank(self)) { 
      returnVal = TRUE;
   } else {
      returnVal = FALSE;
   }
   
   return returnVal;
}

//===== updating neighbor information

/**
\brief Indicate some (non-ACK) packet was received from a neighbor.

This function should be called for each received (non-ACK) packet so neighbor
statistics in the neighbor table can be updated.

The fields which are updated are:
- numRx
- rssi
- asn
- stableNeighbor
- switchStabilityCounter

\param[in] l2_src MAC source address of the packet, i.e. the neighbor who sent
   the packet just received.
\param[in] rssi   RSSI with which this packet was received.
\param[in] asnTs  ASN at which this packet was received.
\param[in] joinPrioPresent Whether a join priority was present in the received
   packet.
\param[in] joinPrio The join priority present in the packet, if any.
*/
void neighbors_indicateRx(OpenMote* self, open_addr_t* l2_src,
                          int8_t       rssi,
                          asn_t*       asnTs,
                          bool         joinPrioPresent,
                          uint8_t      joinPrio) {
   uint8_t i;
   bool    newNeighbor;
   
   // update existing neighbor
   newNeighbor = TRUE;
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ( isThisRowMatching(self, l2_src,i)) {
         
         // this is not a new neighbor
         newNeighbor = FALSE;
         
         // update numRx, rssi, asn
         (self->neighbors_vars).neighbors[i].numRx++;
         (self->neighbors_vars).neighbors[i].rssi=rssi;
         memcpy(&(self->neighbors_vars).neighbors[i].asn,asnTs,sizeof(asn_t));
         //update jp
         if (joinPrioPresent==TRUE){
            (self->neighbors_vars).neighbors[i].joinPrio=joinPrio;
         }
         
         // update stableNeighbor, switchStabilityCounter
         if ((self->neighbors_vars).neighbors[i].stableNeighbor==FALSE) {
            if ((self->neighbors_vars).neighbors[i].rssi>BADNEIGHBORMAXRSSI) {
               (self->neighbors_vars).neighbors[i].switchStabilityCounter++;
               if ((self->neighbors_vars).neighbors[i].switchStabilityCounter>=SWITCHSTABILITYTHRESHOLD) {
                  (self->neighbors_vars).neighbors[i].switchStabilityCounter=0;
                  (self->neighbors_vars).neighbors[i].stableNeighbor=TRUE;
               }
            } else {
               (self->neighbors_vars).neighbors[i].switchStabilityCounter=0;
            }
         } else if ((self->neighbors_vars).neighbors[i].stableNeighbor==TRUE) {
            if ((self->neighbors_vars).neighbors[i].rssi<GOODNEIGHBORMINRSSI) {
               (self->neighbors_vars).neighbors[i].switchStabilityCounter++;
               if ((self->neighbors_vars).neighbors[i].switchStabilityCounter>=SWITCHSTABILITYTHRESHOLD) {
                  (self->neighbors_vars).neighbors[i].switchStabilityCounter=0;
                   (self->neighbors_vars).neighbors[i].stableNeighbor=FALSE;
               }
            } else {
               (self->neighbors_vars).neighbors[i].switchStabilityCounter=0;
            }
         }
         
         // stop looping
         break;
      }
   }
   
   // register new neighbor
   if (newNeighbor==TRUE) {
 registerNewNeighbor(self, l2_src, rssi, asnTs, joinPrioPresent,joinPrio);
   }
}

/**
\brief Indicate some packet was sent to some neighbor.

This function should be called for each transmitted (non-ACK) packet so
neighbor statistics in the neighbor table can be updated.

The fields which are updated are:
- numTx
- numTxACK
- asn

\param[in] l2_dest MAC destination address of the packet, i.e. the neighbor
   who I just sent the packet to.
\param[in] numTxAttempts Number of transmission attempts to this neighbor.
\param[in] was_finally_acked TRUE iff the packet was ACK'ed by the neighbor
   on final transmission attempt.
\param[in] asnTs ASN of the last transmission attempt.
*/
void neighbors_indicateTx(OpenMote* self, open_addr_t* l2_dest,
                          uint8_t      numTxAttempts,
                          bool         was_finally_acked,
                          asn_t*       asnTs) {
   uint8_t i;
   // don't run through this function if packet was sent to broadcast address
   if ( packetfunctions_isBroadcastMulticast(self, l2_dest)==TRUE) {
      return;
   }
   
   // loop through neighbor table
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ( isThisRowMatching(self, l2_dest,i)) {
         // handle roll-over case
        
          if ((self->neighbors_vars).neighbors[i].numTx>(0xff-numTxAttempts)) {
              (self->neighbors_vars).neighbors[i].numWraps++; //counting the number of times that tx wraps.
              (self->neighbors_vars).neighbors[i].numTx/=2;
              (self->neighbors_vars).neighbors[i].numTxACK/=2;
           }
         // update statistics
        (self->neighbors_vars).neighbors[i].numTx += numTxAttempts; 
        
        if (was_finally_acked==TRUE) {
            (self->neighbors_vars).neighbors[i].numTxACK++;
            memcpy(&(self->neighbors_vars).neighbors[i].asn,asnTs,sizeof(asn_t));
        }
        break;
      }
   }
}

/**
\brief Indicate I just received a RPL DIO from a neighbor.

This function should be called for each received a DIO is received so neighbor
routing information in the neighbor table can be updated.

The fields which are updated are:
- DAGrank

\param[in] msg The received message with msg->payload pointing to the DIO
   header.
*/
void neighbors_indicateRxDIO(OpenMote* self, OpenQueueEntry_t* msg) {
   uint8_t          i;
   uint8_t          temp_8b;
  
   // take ownership over the packet
   msg->owner = COMPONENT_NEIGHBORS;
   
   // update rank of that neighbor in table
   (self->neighbors_vars).dio = (icmpv6rpl_dio_ht*)(msg->payload);
   // retrieve rank
   temp_8b            = *(msg->payload+2);
   (self->neighbors_vars).dio->rank = (temp_8b << 8) + *(msg->payload+3);
   if ( isNeighbor(self, &(msg->l2_nextORpreviousHop))==TRUE) {
      for (i=0;i<MAXNUMNEIGHBORS;i++) {
         if ( isThisRowMatching(self, &(msg->l2_nextORpreviousHop),i)) {
            if (
                  (self->neighbors_vars).dio->rank > (self->neighbors_vars).neighbors[i].DAGrank &&
                  (self->neighbors_vars).dio->rank - (self->neighbors_vars).neighbors[i].DAGrank >(DEFAULTLINKCOST*2*MINHOPRANKINCREASE)
               ) {
                // the new DAGrank looks suspiciously high, only increment a bit
                (self->neighbors_vars).neighbors[i].DAGrank += (DEFAULTLINKCOST*2*MINHOPRANKINCREASE);
 openserial_printError(self, COMPONENT_NEIGHBORS,ERR_LARGE_DAGRANK,
                               (errorparameter_t)(self->neighbors_vars).dio->rank,
                               (errorparameter_t)(self->neighbors_vars).neighbors[i].DAGrank);
            } else {
               (self->neighbors_vars).neighbors[i].DAGrank = (self->neighbors_vars).dio->rank;
            }
            break;
         }
      }
   } 
   // update my routing information
 neighbors_updateMyDAGrankAndNeighborPreference(self); 
}

//===== write addresses

/**
\brief Write the 64-bit address of some neighbor to some location.
*/
void neighbors_getNeighbor(OpenMote* self, open_addr_t* address, uint8_t addr_type, uint8_t index){
   switch(addr_type) {
      case ADDR_64B:
         memcpy(&(address->addr_64b),&((self->neighbors_vars).neighbors[index].addr_64b.addr_64b),LENGTH_ADDR64b);
         address->type=ADDR_64B;
         break;
      default:
 openserial_printCritical(self, COMPONENT_NEIGHBORS,ERR_WRONG_ADDR_TYPE,
                               (errorparameter_t)addr_type,
                               (errorparameter_t)1);
         break; 
   }
}

//===== setters

void neighbors_setMyDAGrank(OpenMote* self, dagrank_t rank){
    (self->neighbors_vars).myDAGrank = rank;
}

//===== managing routing info

/**
\brief Update my DAG rank and neighbor preference.

Call this function whenever some data is changed that could cause this mote's
routing decisions to change. Examples are:
- I received a DIO which updated by neighbor table. If this DIO indicated a
  very low DAGrank, I may want to change by routing parent.
- I became a DAGroot, so my DAGrank should be 0.
*/
void neighbors_updateMyDAGrankAndNeighborPreference(OpenMote* self) {
   uint8_t   i;
   uint16_t  rankIncrease;
   uint32_t  tentativeDAGrank; // 32-bit since is used to sum
   uint8_t   prefParentIdx;
   bool      prefParentFound;
   uint32_t  rankIncreaseIntermediary; // stores intermediary results of rankIncrease calculation
   
   // if I'm a DAGroot, my DAGrank is always MINHOPRANKINCREASE
   if (( idmanager_getIsDAGroot(self))==TRUE) {
       // the dagrank is not set through setting command, set rank to MINHOPRANKINCREASE here 
       (self->neighbors_vars).myDAGrank=MINHOPRANKINCREASE;
       return;
   }
   
   // reset my DAG rank to max value. May be lowered below.
   (self->neighbors_vars).myDAGrank  = MAXDAGRANK;
   
   // by default, I haven't found a preferred parent
   prefParentFound           = FALSE;
   prefParentIdx             = 0;
   
   // loop through neighbor table, update myDAGrank
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ((self->neighbors_vars).neighbors[i].used==TRUE) {
         
         // reset parent preference
         (self->neighbors_vars).neighbors[i].parentPreference=0;
         
         // calculate link cost to this neighbor
         if ((self->neighbors_vars).neighbors[i].numTxACK==0) {
            rankIncrease = DEFAULTLINKCOST*2*MINHOPRANKINCREASE;
         } else {
            //6TiSCH minimal draft using OF0 for rank computation
            rankIncreaseIntermediary = (((uint32_t)(self->neighbors_vars).neighbors[i].numTx) << 10);
            rankIncreaseIntermediary = (rankIncreaseIntermediary * 2 * MINHOPRANKINCREASE) / ((uint32_t)(self->neighbors_vars).neighbors[i].numTxACK);
            rankIncrease = (uint16_t)(rankIncreaseIntermediary >> 10);
         }
         
         tentativeDAGrank = (self->neighbors_vars).neighbors[i].DAGrank+rankIncrease;
         if ( tentativeDAGrank<(self->neighbors_vars).myDAGrank &&
              tentativeDAGrank<MAXDAGRANK) {
            // found better parent, lower my DAGrank
            (self->neighbors_vars).myDAGrank   = tentativeDAGrank;
            prefParentFound            = TRUE;
            prefParentIdx              = i;
         }
      }
   } 
   
   // update preferred parent
   if (prefParentFound) {
      (self->neighbors_vars).neighbors[prefParentIdx].parentPreference       = MAXPREFERENCE;
      (self->neighbors_vars).neighbors[prefParentIdx].stableNeighbor         = TRUE;
      (self->neighbors_vars).neighbors[prefParentIdx].switchStabilityCounter = 0;
   }
}

//===== maintenance

void neighbors_removeOld(OpenMote* self) {
   uint8_t    i;
   uint16_t   timeSinceHeard;
   
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ((self->neighbors_vars).neighbors[i].used==1) {
         timeSinceHeard = ieee154e_asnDiff(self, &(self->neighbors_vars).neighbors[i].asn);
         if (timeSinceHeard>DESYNCTIMEOUT) {
 removeNeighbor(self, i);
         }
      }
   } 
}

//===== debug

/**
\brief Triggers this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_neighbors(OpenMote* self) {
   debugNeighborEntry_t temp;
   (self->neighbors_vars).debugRow=((self->neighbors_vars).debugRow+1)%MAXNUMNEIGHBORS;
   temp.row=(self->neighbors_vars).debugRow;
   temp.neighborEntry=(self->neighbors_vars).neighbors[(self->neighbors_vars).debugRow];
 openserial_printStatus(self, STATUS_NEIGHBORS,(uint8_t*)&temp,sizeof(debugNeighborEntry_t));
   return TRUE;
}

//=========================== private =========================================

void registerNewNeighbor(OpenMote* self, open_addr_t* address,
                         int8_t       rssi,
                         asn_t*       asnTimestamp,
                         bool         joinPrioPresent,
                         uint8_t      joinPrio) {
   uint8_t  i,j;
   bool     iHaveAPreferedParent;
   // filter errors
   if (address->type!=ADDR_64B) {
 openserial_printCritical(self, COMPONENT_NEIGHBORS,ERR_WRONG_ADDR_TYPE,
                            (errorparameter_t)address->type,
                            (errorparameter_t)2);
      return;
   }
   // add this neighbor
   if ( isNeighbor(self, address)==FALSE) {
      i=0;
      while(i<MAXNUMNEIGHBORS) {
         if ((self->neighbors_vars).neighbors[i].used==FALSE) {
            // add this neighbor
            (self->neighbors_vars).neighbors[i].used                   = TRUE;
            (self->neighbors_vars).neighbors[i].parentPreference       = 0;
            // (self->neighbors_vars).neighbors[i].stableNeighbor         = FALSE;
            // Note: all new neighbors are consider stable
            (self->neighbors_vars).neighbors[i].stableNeighbor         = TRUE;
            (self->neighbors_vars).neighbors[i].switchStabilityCounter = 0;
            memcpy(&(self->neighbors_vars).neighbors[i].addr_64b,address,sizeof(open_addr_t));
            (self->neighbors_vars).neighbors[i].DAGrank                = DEFAULTDAGRANK;
            (self->neighbors_vars).neighbors[i].rssi                   = rssi;
            (self->neighbors_vars).neighbors[i].numRx                  = 1;
            (self->neighbors_vars).neighbors[i].numTx                  = 0;
            (self->neighbors_vars).neighbors[i].numTxACK               = 0;
            memcpy(&(self->neighbors_vars).neighbors[i].asn,asnTimestamp,sizeof(asn_t));
            //update jp
            if (joinPrioPresent==TRUE){
               (self->neighbors_vars).neighbors[i].joinPrio=joinPrio;
            }
            
            // do I already have a preferred parent ? -- TODO change to use JP
            iHaveAPreferedParent = FALSE;
            for (j=0;j<MAXNUMNEIGHBORS;j++) {
               if ((self->neighbors_vars).neighbors[j].parentPreference==MAXPREFERENCE) {
                  iHaveAPreferedParent = TRUE;
               }
            }
            // if I have none, and I'm not DAGroot, the new neighbor is my preferred
            if (iHaveAPreferedParent==FALSE && idmanager_getIsDAGroot(self)==FALSE) {      
               (self->neighbors_vars).neighbors[i].parentPreference     = MAXPREFERENCE;
            }
            break;
         }
         i++;
      }
      if (i==MAXNUMNEIGHBORS) {
 openserial_printError(self, COMPONENT_NEIGHBORS,ERR_NEIGHBORS_FULL,
                               (errorparameter_t)MAXNUMNEIGHBORS,
                               (errorparameter_t)0);
         return;
      }
   }
}

bool isNeighbor(OpenMote* self, open_addr_t* neighbor) {
   uint8_t i=0;
   for (i=0;i<MAXNUMNEIGHBORS;i++) {
      if ( isThisRowMatching(self, neighbor,i)) {
         return TRUE;
      }
   }
   return FALSE;
}

void removeNeighbor(OpenMote* self, uint8_t neighborIndex) {
   (self->neighbors_vars).neighbors[neighborIndex].used                      = FALSE;
   (self->neighbors_vars).neighbors[neighborIndex].parentPreference          = 0;
   (self->neighbors_vars).neighbors[neighborIndex].stableNeighbor            = FALSE;
   (self->neighbors_vars).neighbors[neighborIndex].switchStabilityCounter    = 0;
   //(self->neighbors_vars).neighbors[neighborIndex].addr_16b.type           = ADDR_NONE; // to save RAM
   (self->neighbors_vars).neighbors[neighborIndex].addr_64b.type             = ADDR_NONE;
   //(self->neighbors_vars).neighbors[neighborIndex].addr_128b.type          = ADDR_NONE; // to save RAM
   (self->neighbors_vars).neighbors[neighborIndex].DAGrank                   = DEFAULTDAGRANK;
   (self->neighbors_vars).neighbors[neighborIndex].rssi                      = 0;
   (self->neighbors_vars).neighbors[neighborIndex].numRx                     = 0;
   (self->neighbors_vars).neighbors[neighborIndex].numTx                     = 0;
   (self->neighbors_vars).neighbors[neighborIndex].numTxACK                  = 0;
   (self->neighbors_vars).neighbors[neighborIndex].asn.bytes0and1            = 0;
   (self->neighbors_vars).neighbors[neighborIndex].asn.bytes2and3            = 0;
   (self->neighbors_vars).neighbors[neighborIndex].asn.byte4                 = 0;
}

//=========================== helpers =========================================

bool isThisRowMatching(OpenMote* self, open_addr_t* address, uint8_t rowNumber) {
   switch (address->type) {
      case ADDR_64B:
         return (self->neighbors_vars).neighbors[rowNumber].used &&
 packetfunctions_sameAddress(self, address,&(self->neighbors_vars).neighbors[rowNumber].addr_64b);
      default:
 openserial_printCritical(self, COMPONENT_NEIGHBORS,ERR_WRONG_ADDR_TYPE,
                               (errorparameter_t)address->type,
                               (errorparameter_t)3);
         return FALSE;
   }
}