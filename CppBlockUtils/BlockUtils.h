////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011, Alan C. Reiner    <alan.reiner@gmail.com>             //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE or http://www.gnu.org/licenses/agpl.html                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _BLOCKUTILS_H_
#define _BLOCKUTILS_H_

#include <stdio.h>
#include <iostream>
//#ifdef WIN32
//#include <cstdint>
//#else
//#include <stdlib.h>
//#include <inttypes.h>
//#include <cstring>
//#endif
#include <fstream>
#include <vector>
#include <queue>
#include <deque>
#include <list>
#include <bitset>
#include <map>
#include <set>
#include <limits>

#include "BinaryData.h"
#include "BtcUtils.h"
#include "BlockObj.h"
#include "BlockObjRef.h"

#include "cryptlib.h"
#include "sha.h"
#include "UniversalTimer.h"



#define TX_0_UNCONFIRMED    0 
#define TX_NOT_EXIST       -1
#define TX_OFF_MAIN_BRANCH -2


using namespace std;

class BlockDataManager_FullRAM;



////////////////////////////////////////////////////////////////////////////////
// TxIOPair
//
// This makes a lot of sense, despite the added complexity.  No TxIn exists
// unless there was a TxOut, and they both have the same value, so we will
// store them together here.
//
// This will provide the future benefit of easily determining what Tx data
// can be pruned.  If a TxIOPair has both TxIn and TxOut, then the value 
// was received and spent, contributes zero to our balance, and can effectively
// ignored.  For now we will maintain them, but in the future we may decide
// to just start removing TxIOPairs after they are spent...
//
//
class TxIOPair
{
public:
   //////////////////////////////////////////////////////////////////////////////
   // TODO:  since we tend not to track TxIn/TxOuts but make them on the fly,
   //        we should probably do that here, too.  I designed this before I
   //        realized that these copies will fall out of sync on a reorg
   TxIOPair(void);
   TxIOPair(uint64_t  amount);
   TxIOPair(TxRef* txPtrO, uint32_t txoutIndex);
   TxIOPair(TxRef* txPtrO, uint32_t txoutIndex, TxRef* txPtrI, uint32_t txinIndex);

   // Lots of accessors
   bool      hasTxOut(void) const   { return (txPtrOfOutput_   != NULL); }
   bool      hasTxIn(void) const    { return (txPtrOfInput_    != NULL); }
   bool      hasTxOutZC(void) const { return (txPtrOfOutputZC_ != NULL); }
   bool      hasTxInZC(void) const  { return (txPtrOfInputZC_  != NULL); }
   bool      hasValue(void) const   { return (amount_!=0); }
   uint64_t  getValue(void) const   { return  amount_;}

   //////////////////////////////////////////////////////////////////////////////
   TxOutRef  getTxOutRef(void) const;   
   TxInRef   getTxInRef(void) const;   
   TxOutRef  getTxOutRefZC(void) const {return txPtrOfOutputZC_->getTxOutRef(indexOfOutputZC_);}
   TxInRef   getTxInRefZC(void) const  {return txPtrOfInputZC_->getTxInRef(indexOfInputZC_);}
   TxRef&    getTxRefOfOutput(void) const { return *txPtrOfOutput_; }
   TxRef&    getTxRefOfInput(void) const  { return *txPtrOfInput_;  }
   OutPoint  getOutPoint(void) { return OutPoint(getTxHashOfOutput(),indexOfOutput_);}

   pair<bool,bool> reassessValidity(void);
   bool isSentToSelf(void)  { return isSentToSelf_; }
   void setSentToSelf(bool isTrue=true) { isSentToSelf_ = isTrue; }


   //////////////////////////////////////////////////////////////////////////////
   BinaryData    getTxHashOfInput(void);
   BinaryData    getTxHashOfOutput(void);

   bool setTxInRef (TxRef* txref, uint32_t index, bool isZeroConf=false);
   bool setTxOutRef(TxRef* txref, uint32_t index, bool isZeroConf=false);

   //////////////////////////////////////////////////////////////////////////////
   bool isSourceUnknown(void) { return ( !hasTxOut() &&  hasTxIn() ); }
   bool isStandardTxOutScript(void);

   bool isSpent(void);
   bool isUnspent(void);
   bool isSpendable(void);      
   bool isMineButUnconfirmed(uint32_t currBlk, uint32_t minConf=6);
   void clearZCFields(void);

private:
   uint64_t  amount_;
   TxRef*    txPtrOfOutput_;
   uint32_t  indexOfOutput_;
   TxRef*    txPtrOfInput_;
   uint32_t  indexOfInput_;

   TxRef*    txPtrOfOutputZC_;
   uint32_t  indexOfOutputZC_;
   TxRef*    txPtrOfInputZC_;
   uint32_t  indexOfInputZC_;

   bool      isSentToSelf_;
};


////////////////////////////////////////////////////////////////////////////////
//
// LedgerEntry  
//
// LedgerEntry class is used for bother BtcAddresses and BtcWallets.  Members
// have slightly different meanings (or irrelevant) depending which one it's
// used with.
//
//  BtcAddress -- Each entry corresponds to ONE TxIn OR ONE TxOut
//
//    addr20_    -  useless - just repeating this address
//    value_     -  net debit/credit on addr balance, in Satoshis (1e-8 BTC)
//    blockNum_  -  block height of the tx in which this txin/out was included
//    txHash_    -  hash of the tx in which this txin/txout was included
//    index_     -  index of the txin/txout in this tx
//    isValid_   -  default to true -- invalidated due to reorg/double-spend
//    isSentToSelf_ - if this is a txOut, did it come from ourself?
//    isChangeBack_ - meaningless:  can't quite figure out how to determine
//                    this unless I do a prescan to determine if all txOuts
//                    are ours, or just some of them
//
//  BtcWallet -- Each entry corresponds to ONE WHOLE TRANSACTION
//
//    addr20_    -  useless - originally had a purpose, but lost it
//    value_     -  total debit/credit on WALLET balance, in Satoshis (1e-8 BTC)
//    blockNum_  -  block height of the block in which this tx was included
//    txHash_    -  hash of this tx 
//    index_     -  index of the tx in the block
//    isValid_   -  default to true -- invalidated due to reorg/double-spend
//    isSentToSelf_ - if we supplied inputs and rx ALL outputs
//    isChangeBack_ - if we supplied inputs and rx ANY outputs
//
////////////////////////////////////////////////////////////////////////////////
class LedgerEntry
{
public:
   LedgerEntry(void) :
      addr20_(0),
      value_(0),
      blockNum_(UINT32_MAX),
      txHash_(BtcUtils::EmptyHash_),
      index_(UINT32_MAX),
      txTime_(0),
      isValid_(false),
      isSentToSelf_(false),
      isChangeBack_(false) {}

   LedgerEntry(BinaryData const & addr20,
               int64_t val, 
               uint32_t blkNum, 
               BinaryData const & txhash, 
               uint32_t idx,
               uint64_t txtime=0,
               bool isToSelf=false,
               bool isChange=false) :
      addr20_(addr20),
      value_(val),
      blockNum_(blkNum),
      txHash_(txhash),
      index_(idx),
      txTime_(txtime),
      isValid_(true),
      isSentToSelf_(isToSelf),
      isChangeBack_(isChange) {}

   BinaryData const &  getAddrStr20(void) const { return addr20_;        }
   int64_t             getValue(void) const     { return value_;         }
   uint32_t            getBlockNum(void) const  { return blockNum_;      }
   BinaryData const &  getTxHash(void) const    { return txHash_;        }
   uint32_t            getIndex(void) const     { return index_;         }
   uint32_t            getTxTime(void) const    { return txTime_;        }
   bool                isValid(void) const      { return isValid_;       }
   bool                isSentToSelf(void) const { return isSentToSelf_;  }
   bool                isChangeBack(void) const { return isChangeBack_;  }

   void setAddr20(BinaryData const & bd) { addr20_.copyFrom(bd); }
   void setValid(bool b=true) { isValid_ = b; }
   void changeBlkNum(uint32_t newHgt) {blockNum_ = newHgt; }
      
   bool operator<(LedgerEntry const & le2) const;
   bool operator==(LedgerEntry const & le2) const;

   void pprint(void);
   void pprintOneLine(void);

private:
   

   BinaryData       addr20_;
   int64_t          value_;
   uint32_t         blockNum_;
   BinaryData       txHash_;
   uint32_t         index_;  // either a tx index, txout index or txin index
   uint64_t         txTime_;
   bool             isValid_;
   bool             isSentToSelf_;
   bool             isChangeBack_;;


   
}; 


////////////////////////////////////////////////////////////////////////////////
//
// BtcAddress  
//
// This class is only for scanning the blockchain (information only).  It has
// no need to keep track of the public and private keys of various addresses,
// which is done by the python code leveraging this class.
////////////////////////////////////////////////////////////////////////////////
class BtcAddress
{
public:

   BtcAddress(void) : 
      address20_(0), firstBlockNum_(0), firstTimestamp_(0), 
      lastBlockNum_(0), lastTimestamp_(0), 
      relevantTxIOPtrs_(0), ledger_(0) {}

   BtcAddress(BinaryData    addr, 
              uint32_t      firstBlockNum = 0,
              uint32_t      firstTimestamp = 0,
              uint32_t      lastBlockNum = 0,
              uint32_t      lastTimestamp = 0);
   
   BinaryData const &  getAddrStr20(void) const  {return address20_;      }
   uint32_t       getFirstBlockNum(void) const   {return firstBlockNum_;  }
   uint32_t       getFirstTimestamp(void) const  {return firstTimestamp_; }
   uint32_t       getLastBlockNum(void)          {return lastBlockNum_;   }
   uint32_t       getLastTimestamp(void)         {return lastTimestamp_;  }
   void           setFirstBlockNum(uint32_t b)   { firstBlockNum_  = b; }
   void           setFirstTimestamp(uint32_t t)  { firstTimestamp_ = t; }
   void           setLastBlockNum(uint32_t b)    { lastBlockNum_   = b; }
   void           setLastTimestamp(uint32_t t)   { lastTimestamp_  = t; }

   bool           isUnused(void) { return (firstBlockNum_==UINT32_MAX &&
                                           firstTimestamp_==UINT32_MAX); }
   void           setUnused(void) {firstBlockNum_=UINT32_MAX; 
                                   firstTimestamp_=UINT32_MAX;}

   void           setAddrStr20(BinaryData bd)    { address20_.copyFrom(bd);}

   void     sortLedger(void);
   uint32_t removeInvalidEntries(void);

   // BlkNum is necessary for "unconfirmed" list, since it is dependent
   // on number of confirmations.  But for "spendable" TxOut list, it is
   // only a convenience, if you want to be able to calculate numConf from
   // the Utxos in the list.  If you don't care (i.e. you only want to 
   // know what TxOuts are available to spend, you can pass in 0 for currBlk
   uint64_t getFullBalance(void);
   uint64_t getSpendableBalance(void);
   uint64_t getUnconfirmedBalance(uint32_t currBlk);
   vector<UnspentTxOut> getFullTxOutList(uint32_t currBlk=0);
   vector<UnspentTxOut> getSpendableTxOutList(uint32_t currBlk=0);
   void clearZeroConfPool(void);


   vector<LedgerEntry> & getTxLedger(void)       { return ledger_;   }
   vector<LedgerEntry> & getZeroConfLedger(void) { return ledgerZC_; }

   vector<TxIOPair*> &   getTxIOList(void) { return relevantTxIOPtrs_; }

   void addTxIO(TxIOPair * txio, bool isZeroConf=false);
   void addTxIO(TxIOPair & txio, bool isZeroConf=false);
   void addLedgerEntry(LedgerEntry const & le, bool isZeroConf=false); 

   void pprintLedger(void);

private:
   BinaryData address20_;
   uint32_t   firstBlockNum_;
   uint32_t   firstTimestamp_;
   uint32_t   lastBlockNum_;
   uint32_t   lastTimestamp_;

   // Each address will store a list of pointers to its transactions
   vector<TxIOPair*>     relevantTxIOPtrs_;
   vector<TxIOPair*>     relevantTxIOPtrsZC_;
   vector<LedgerEntry>   ledger_;
   vector<LedgerEntry>   ledgerZC_;
};




////////////////////////////////////////////////////////////////////////////////
//
// BtcWallet
//
////////////////////////////////////////////////////////////////////////////////
class BtcWallet
{


public:
   BtcWallet(void) {}

   /////////////////////////////////////////////////////////////////////////////
   void addAddress(BtcAddress const & newAddr);
   void addAddress(BinaryData    addr, 
                   uint32_t      firstTimestamp = 0,
                   uint32_t      firstBlockNum  = 0,
                   uint32_t      lastTimestamp = 0,
                   uint32_t      lastBlockNum  = 0);

   // SWIG has some serious problems with typemaps and variable arg lists
   // Here I just create some extra functions that sidestep all the problems
   // but it would be nice to figure out "typemap typecheck" in SWIG...
   void addAddress_BtcAddress_(BtcAddress const & newAddr);

   void addAddress_1_(BinaryData    addr);

   void addAddress_3_(BinaryData    addr, 
                      uint32_t      firstTimestamp,
                      uint32_t      firstBlockNum);

   void addAddress_5_(BinaryData    addr, 
                      uint32_t      firstTimestamp,
                      uint32_t      firstBlockNum,
                      uint32_t      lastTimestamp,
                      uint32_t      lastBlockNum);

   bool hasAddr(BinaryData const & addr20);


   // Scan a Tx for our TxIns/TxOuts.  Override default blk vals if you think
   // you will save time by not checking addresses that are much newr than
   // the block
   void       scanTx(TxRef & tx, 
                     uint32_t txIndex = UINT32_MAX,
                     uint32_t blktime = UINT32_MAX,
                     uint32_t blknum = UINT32_MAX);

   void       scanNonStdTx(uint32_t blknum, 
                           uint32_t txidx, 
                           TxRef&   txref,
                           uint32_t txoutidx,
                           BtcAddress& addr);


   // BlkNum is necessary for "unconfirmed" list, since it is dependent
   // on number of confirmations.  But for "spendable" TxOut list, it is
   // only a convenience, if you want to be able to calculate numConf from
   // the Utxos in the list.  If you don't care (i.e. you only want to 
   // know what TxOuts are available to spend, you can pass in 0 for currBlk
   uint64_t getFullBalance(void);
   uint64_t getSpendableBalance(void);
   uint64_t getUnconfirmedBalance(uint32_t currBlk);
   vector<UnspentTxOut> getFullTxOutList(uint32_t currBlk=0);
   vector<UnspentTxOut> getSpendableTxOutList(uint32_t currBlk=0);
   void clearZeroConfPool(void);

   
   uint32_t     getNumAddr(void) {return addrMap_.size();}
   BtcAddress & getAddrByIndex(uint32_t i) { return *(addrPtrVect_[i]); }
   BtcAddress & getAddrByHash160(BinaryData const & a) { return addrMap_[a];}

   void     sortLedger(void);
   uint32_t removeInvalidEntries(void);

   vector<LedgerEntry>       getZeroConfLedger(BinaryData const * addr160=NULL);
   vector<LedgerEntry>       getTxLedger(BinaryData const * addr160=NULL); 
   map<OutPoint, TxIOPair> & getTxIOMap(void)    {return txioMap_;}
   map<OutPoint, TxIOPair> & getNonStdTxIO(void) {return nonStdTxioMap_;}

   bool isOutPointMine(BinaryData const & hsh, uint32_t idx);

   void pprintLedger(void);
   void pprintAlot(void);

   //map<OutPoint,TxOutRef> & getMyZeroConfTxOuts(void) {return myZeroConfTxOuts_;}
   //set<OutPoint> & getMyZeroConfOutPointsToSelf(void) {return myZeroConfOutPointsToSelf_;}

private:
   vector<BtcAddress*>          addrPtrVect_;
   map<BinaryData, BtcAddress>  addrMap_;
   map<OutPoint, TxIOPair>      txioMap_;


   vector<LedgerEntry>          ledgerAllAddr_;  
   vector<LedgerEntry>          ledgerAllAddrZC_;  

   set<OutPoint>                lockedTxOuts_;
   set<OutPoint>                orphanTxIns_;
   set<TxRef*>                  txrefSet_;      // aggregation of all relevant Tx

   // For non-std transactions
   map<OutPoint, TxIOPair>      nonStdTxioMap_;
   set<OutPoint>                nonStdUnspentOutPoints_;
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class ZeroConfData
{
public:
   TxRef         txref_;   
   uint64_t      txtime_;
   list<BinaryData>::iterator iter_;

};





// Some might argue that inheritance would be useful here.  I'm not a software
// guy, and I have to write all the methods for each class anyway.  So I'm 
// foregoing the inheritance.  Just writing each class separately

// FullRAM BDM:
//    Very few use cases, and will be nearly impossible if transaction volume
//    picks up at all.  However, if you need to do TONS of computation on the
//    blockchain very quickly, and you have the RAM, this may be useful for you
//    Headers and BlockData/Tx stored in the same structure
//class BlockDataManager_FullRAM;

// FullHDD BDM:
//    This is the standard full-blockchain node.  It pulls all the blockdata
//    into memory only to scan it and index it.  After that, it stores byte
//    locations of the block chain, and reads the data from file on demand.
//    Headers and BlockData/Tx stored in the same structure
//class BlockDataManager_FullHDD;

// Medium BDM:
//    No storage of the blockchain, only the headers and TxOut/TxIn lists 
//    are stored in memory and blocks data is pulled from the network as needed
//    Headers are stored in their own compact structure
//class BlockDataManager_Medium;

// Connection BDM:
//    Basically a proxy to a BDM on another system to be accessed via sockets
//    This may not actually be needed, as it would probably be easier to 
//    implement in python
//class BlockDataManager_ServerConnection;


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// The goal of this class is to create a memory pool in RAM that looks exactly
// the same as the block-headers storage on disk.  There is no serialization
// or unserialization, we just copy back and forth between disk and RAM, and 
// we're done.  So it should be about as fast as theoretically possible, you 
// are limited only by your disk I/O speed.
//
// This is more of a simple test, which will later be applied to the entire
// blockchain.  If it works as expected, then this will potentially be useful
// for the official BTC client which seems to have some speed problems at 
// startup and shutdown.
//
// This class is a singleton -- there can only ever be one, accessed through
// the static method GetInstance().  This method gets the single instantiation
// of the BDM class, and then its public members can be used to access the 
// block data that is sitting in memory.
//
typedef enum
{
   BDM_MODE_FULL_BLOCKCHAIN,
   BDM_MODE_LIGHT_STORAGE,
   BDM_MODE_NO_STORAGE,
   BDM_MODE_COUNT
}  BDM_MODE;


typedef enum
{
  ADD_BLOCK_SUCCEEDED,
  ADD_BLOCK_NEW_TOP_BLOCK,
  ADD_BLOCK_CAUSED_REORG,
} ADD_BLOCK_RESULT_INDEX;



class BlockDataManager_FullRAM;





////////////////////////////////////////////////////////////////////////////////
//
// BlockDataManager is a SINGLETON:  only one is ever created.  
//
// Access it via BlockDataManager_FullRAM::GetInstance();
//
////////////////////////////////////////////////////////////////////////////////
class BlockDataManager_FullRAM
{
private:

   // These four data structures contain all the *real* data.  Everything 
   // else is just references and pointers to this data
   string                             blkfilePath_;
   BinaryData                         blockchainData_ALL_;
   list<BinaryData>                   blockchainData_NEW_; 
   map<HashString, BlockHeaderRef>    headerHashMap_;
   map<HashString, TxRef>             txHashMap_;

   // Need a separate memory pool just for zero-confirmation transactions
   // We need the second map to make sure we can find the data to remove
   // it, when necessary
   list<BinaryData>                   zeroConfTxList_;
   map<HashString, ZeroConfData>      zeroConfMap_;
   bool                               zcEnabled_;
   string                             zcFilename_;

   // This is for detecting external changes made to the blk0001.dat file
   uint64_t                           lastEOFByteLoc_;
   uint64_t                           totalBlockchainBytes_;

   // If we are really ambitious and have a lot of RAM, we might save
   // all addresses for super-fast lookup.  The key is the 20B addr
   // and the value is a vector of tx-hashes that include this addr
   map<BinaryData, set<HashString> >  allAddrTxMap_;
   bool                               isAllAddrLoaded_;

   // For the case of keeping tx/header data on disk:
   vector<string>                     blockchainFilenames_;
   map<HashString, pair<int,int> >    txFileRefs_;
   map<HashString, pair<int,int> >    headerFileRefs_;


   // These should be set after the blockchain is organized
   deque<BlockHeaderRef*>            headersByHeight_;
   BlockHeaderRef*                   topBlockPtr_;
   BlockHeaderRef*                   genBlockPtr_;

   // Reorganization details
   bool                              lastBlockWasReorg_;
   BlockHeaderRef*                   reorgBranchPoint_;
   BlockHeaderRef*                   prevTopBlockPtr_;
   set<HashString>                   txJustInvalidated_;
   set<HashString>                   txJustAffected_;

   // Store info on orphan chains
   vector<BlockHeaderRef*>           previouslyValidBlockHeaderPtrs_;
   vector<BlockHeaderRef*>           orphanChainStartBlocks_;

   static BlockDataManager_FullRAM* theOnlyBDM_;
   static bool bdmCreatedYet_;
   bool isInitialized_;


   // These will be set for the specific network we are testing
   BinaryData GenesisHash_;
   BinaryData GenesisTxHash_;
   BinaryData MagicBytes_;


private:
   // Set the constructor to private so that only one can ever be created
   BlockDataManager_FullRAM(void);

public:

   static BlockDataManager_FullRAM & GetInstance(void);
   bool isInitialized(void) { return isInitialized_;}
   void SetBtcNetworkParams( BinaryData const & GenHash,
                             BinaryData const & GenTxHash,
                             BinaryData const & MagicBytes);
   void SelectNetwork(string netName);

   /////////////////////////////////////////////////////////////////////////////
   void Reset(void);
   int32_t          getNumConfirmations(BinaryData txHash);
   BlockHeaderRef & getTopBlockHeader(void) ;
   BlockHeaderRef & getGenesisBlock(void) ;
   BlockHeaderRef * getHeaderByHeight(int index);
   BlockHeaderRef * getHeaderByHash(BinaryData const & blkHash);
   TxRef *          getTxByHash(BinaryData const & txHash);
   string           getBlockfilePath(void) {return blkfilePath_;}


   // Parsing requires the data TO ALREADY BE IN ITS PERMANENT MEMORY LOCATION
   bool             parseNewBlockData(BinaryRefReader & rawBlockDataReader,
                                      uint64_t & currBlockchainSize);

   // When we add new block data, we will need to store/copy it to its
   // permanent memory location before parsing it.
   // These methods return (blockAddSucceeded, newBlockIsTop, didCauseReorg)
   vector<bool>     addNewBlockData(   BinaryData rawBlockDataCopy,
                                       bool writeToBlk0001=false);
   vector<bool>     addNewBlockDataRef(BinaryDataRef nonPermBlockDataRef,
                                       bool writeToBlk0001=false);

   void             reassessAfterReorg(BlockHeaderRef* oldTopPtr,
                                       BlockHeaderRef* newTopPtr,
                                       BlockHeaderRef* branchPtr );

   bool             hasTxWithHash(BinaryData const & txhash,
                                  bool includeZeroConf=true) const;
   bool             hasHeaderWithHash(BinaryData const & txhash) const;
   uint32_t         getNumBlocks(void) const { return headerHashMap_.size(); }
   uint32_t         getNumTx(void) const { return txHashMap_.size(); }
   vector<BlockHeaderRef*> getHeadersNotOnMainChain(void);

   // Prefix searches would be much better if we had an some kind of underlying
   // Trie/PatriciaTrie/DeLaBrandiaTrie instead of std::map<>.  For now this
   // search will simply be suboptimal...
   vector<BlockHeaderRef*> prefixSearchHeaders(BinaryData const & searchStr);
   vector<TxRef*>          prefixSearchTx     (BinaryData const & searchStr);
   vector<BinaryData>      prefixSearchAddress(BinaryData const & searchStr);

   // Traverse the blockchain and update the wallet[s] with the relevant Tx data
   void scanBlockchainForTx(BtcWallet & myWallet,
                            uint32_t startBlknum=0,
                            uint32_t endBlknum=0xffffffff);
   void scanBlockchainForTx(vector<BtcWallet*> walletVect,
                            uint32_t startBlknum=0,
                            uint32_t endBlknum=0xffffffff);
 
   // This is extremely slow and RAM-hungry, but may be useful on occasion
   uint32_t       readBlkFile_FromScratch(string filename, bool doOrganize=true);
   uint32_t       readBlkFileUpdate(string filename="");
   bool           verifyBlkFileIntegrity(void);
   void           scanBlockchainForTx_FromScratch_AllAddr(void);
   vector<TxRef*> findAllNonStdTx(void);
   

   // For zero-confirmation tx-handling
   void enableZeroConf(string);
   void disableZeroConf(string);
   void readZeroConfFile(string);
   bool addNewZeroConfTx(BinaryData const & rawTx, uint64_t txtime, bool writeToFile);
   void purgeZeroConfPool(void);
   void pprintZeroConfPool(void);
   void rewriteZeroConfFile(void);
   void rescanWalletZeroConf(BtcWallet & wlt);


   // After reading in all headers, find the longest chain and set nextHash vals
   // TODO:  Figure out if there is an elegant way to deal with a forked 
   //        blockchain containing two equal-length chains
   bool organizeChain(bool forceRebuild=false);

   /////////////////////////////////////////////////////////////////////////////
   bool             isLastBlockReorg(void)     {return lastBlockWasReorg_;}
   set<HashString>  getTxJustInvalidated(void) {return txJustInvalidated_;}
   set<HashString>  getTxJustAffected(void)    {return txJustAffected_;}
   void             updateWalletAfterReorg(BtcWallet & wlt);
   void             updateWalletsAfterReorg(vector<BtcWallet*> wlt);

   // Use these two methods to get ALL information about your unused TxOuts
   //vector<UnspentTxOut> getUnspentTxOutsForWallet(BtcWallet & wlt, int sortType=-1);
   //vector<UnspentTxOut> getNonStdUnspentTxOutsForWallet(BtcWallet & wlt);

   ////////////////////////////////////////////////////////////////////////////////
   // We're going to need the BDM's help to get the sender for a TxIn since it
   // sometimes requires going and finding the TxOut from the distant past
   TxOutRef   getPrevTxOut(TxInRef & txin);
   BinaryData getSenderAddr20(TxInRef & txin);
   int64_t    getSentValue(TxInRef & txin);

private:

   /////////////////////////////////////////////////////////////////////////////
   // Start from a node, trace down to the highest solved block, accumulate
   // difficulties and difficultySum values.  Return the difficultySum of 
   // this block.
   double traceChainDown(BlockHeaderRef & bhpStart);
   void   markOrphanChain(BlockHeaderRef & bhpStart);


   
};


////////////////////////////////////////////////////////////////////////////////
//
// We have a problem with "classic" swig refusing to compile static functions,
// which gives me no way to access BDM which is a singleton class accessed by
// a static class method.  This class simply wraps the call to be invoked in
// python/swig
//
////////////////////////////////////////////////////////////////////////////////
class BlockDataManager
{
public:
   BlockDataManager(void) { bdm_ = &(BlockDataManager_FullRAM::GetInstance());}
   
   BlockDataManager_FullRAM & getBDM(void) { return *bdm_; }

private:
   BlockDataManager_FullRAM* bdm_;
};


#endif
