////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011, Alan C. Reiner    <alan.reiner@gmail.com>             //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE or http://www.gnu.org/licenses/agpl.html                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

#include "BinaryData.h"
#include "BtcUtils.h"
#include "BlockObj.h"
#include "BlockObjRef.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// BlockHeader methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void BlockHeader::serialize(BinaryWriter & bw)
{
   bw.put_uint32_t  ( version_     );
   bw.put_BinaryData( prevHash_    );
   bw.put_BinaryData( merkleRoot_  );
   bw.put_uint32_t  ( timestamp_   );
   bw.put_BinaryData( diffBits_    );
   bw.put_uint32_t  ( nonce_       );
}

/////////////////////////////////////////////////////////////////////////////
BinaryData BlockHeader::serialize(void)
{
   BinaryWriter bw(HEADER_SIZE);
   serialize(bw);
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(uint8_t const * start, BinaryData const * suppliedHash)
{
   version_ =     *(uint32_t*)( start +  0     );
   prevHash_.copyFrom         ( start +  4, 32 );
   merkleRoot_.copyFrom       ( start + 36, 32 );
   timestamp_ =   *(uint32_t*)( start + 68     );
   diffBits_.copyFrom         ( start + 72, 4  );
   nonce_ =       *(uint32_t*)( start + 76     );

   if(suppliedHash==NULL)
      BtcUtils::getHash256(start, HEADER_SIZE, thisHash_);
   else
      thisHash_.copyFrom(*suppliedHash);
   difficultyDbl_ = BtcUtils::convertDiffBitsToDouble( diffBits_.getRef() );
}

/////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(BinaryReader & br)
{
   BinaryData str;
   br.get_BinaryData(str, HEADER_SIZE);
   unserialize(str);
}

/////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(string const & str)
{
   BinaryDataRef bdr((uint8_t const *)str.c_str(), str.size());
   unserialize(bdr);
} 

/////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(BinaryData const & str)
{
   unserialize(str.getPtr());
} 

/////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(BinaryDataRef const & str)
{
   unserialize(str.getPtr());
} 


/////////////////////////////////////////////////////////////////////////////
BlockHeader::BlockHeader( uint8_t const * bhDataPtr, BinaryData* thisHash) :
   prevHash_(32),
   difficultyDbl_(-1.0),
   nextHash_(32),
   blockHeight_(0),
   blkByteLoc_(0),  
   difficultySum_(-1.0),
   isMainBranch_(false),
   isOrphan_(false),
   isFinishedCalc_(false)
{
   unserialize(bhDataPtr, thisHash);
}

/////////////////////////////////////////////////////////////////////////////
BlockHeader::BlockHeader(BinaryData const & header80B)
{
   unserialize(header80B);
}

/////////////////////////////////////////////////////////////////////////////
BlockHeader::BlockHeader( BinaryData const * serHeader ,
             BinaryData const * suppliedHash,
             uint64_t           fileLoc) :
   prevHash_(32),
   thisHash_(32),
   difficultyDbl_(-1.0),
   nextHash_(32),
   blockHeight_(0),
   blkByteLoc_(fileLoc),  
   difficultySum_(-1.0),
   isMainBranch_(false),
   isOrphan_(false),
   isFinishedCalc_(false),
   isOnDiskYet_(false)
{
   if(serHeader != NULL)
   {
      unserialize(*serHeader);
      if( suppliedHash != NULL )
         thisHash_.copyFrom(*suppliedHash);
      else
         BtcUtils::getHash256(*serHeader, thisHash_);
   }
}



/////////////////////////////////////////////////////////////////////////////
void BlockHeader::pprint(ostream & os, int nIndent, bool pBigendian) const
{
   string indent = "";
   for(int i=0; i<nIndent; i++)
      indent = indent + "   ";

   string endstr = (pBigendian ? " (BE)" : " (LE)");
   os << indent << "Block Information: " << blockHeight_ << endl;
   os << indent << "   Hash:       " 
                << thisHash_.toHexStr(pBigendian).c_str() << endstr << endl;
   os << indent << "   Timestamp:  " << getTimestamp() << endl;
   os << indent << "   Prev Hash:  " 
                << prevHash_.toHexStr(pBigendian).c_str() << endstr << endl;
   os << indent << "   MerkleRoot: " 
                << getMerkleRoot().toHexStr(pBigendian).c_str() << endstr << endl;
   os << indent << "   Difficulty: " << (difficultyDbl_)
                         << "    (" << getDiffBits().toHexStr().c_str() << ")" << endl;
   os << indent << "   CumulDiff:  " << (difficultySum_) << endl;
   os << indent << "   Nonce:      " << getNonce() << endl;
   os << indent << "   FileOffset: " << blkByteLoc_ << endl;
}

////////////////////////////////////////////////////////////////////////////////
// For now, I just want to create difficulty-1 blocks
uint32_t BlockHeader::findNonce(void)
{
   BinaryData playHeader(serialize());
   BinaryData fourZeros = BinaryData::CreateFromHex("00000000");
   BinaryData hashResult(32);
   for(uint32_t nonce=0; nonce<(uint32_t)(-1); nonce++)
   {
      *(uint32_t*)(playHeader.getPtr()+76) = nonce;
      BtcUtils::getHash256_NoSafetyCheck(playHeader.getPtr(), HEADER_SIZE, hashResult);
      if(hashResult.getSliceRef(28,4) == fourZeros)
      {
         cout << "NONCE FOUND! " << nonce << endl;
         unserialize(playHeader);
         cout << "Raw Header: " << serialize().toHexStr() << endl;
         pprint();
         cout << "Hash:       " << hashResult.toHexStr() << endl;
         return nonce;
      }

      if(nonce % 10000000 == 0)
      {
         cout << ".";
         cout.flush();
      }
   }
   cout << "No nonce found!" << endl;
   return 0;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// OutPoint methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Define these operators so that we can use OutPoint as a map<> key
bool OutPoint::operator<(OutPoint const & op2) const
{
   if(txHash_ == op2.txHash_)
      return txOutIndex_ < op2.txOutIndex_;
   else
      return txHash_ < op2.txHash_;
}
bool OutPoint::operator==(OutPoint const & op2) const
{
   return (txHash_ == op2.txHash_ && txOutIndex_ == op2.txOutIndex_);
}

void OutPoint::serialize(BinaryWriter & bw)
{
   bw.put_BinaryData(txHash_);
   bw.put_uint32_t(txOutIndex_);
}

BinaryData OutPoint::serialize(void)
{
   BinaryWriter bw(36);
   serialize(bw);
   return bw.getData();
}

void OutPoint::unserialize(uint8_t const * ptr)
{
   txHash_.copyFrom(ptr, 32);
   txOutIndex_ = *(uint32_t*)(ptr+32);
}
void OutPoint::unserialize(BinaryReader & br)
{
   br.get_BinaryData(txHash_, 32);
   txOutIndex_ = br.get_uint32_t();
}
void OutPoint::unserialize(BinaryRefReader & brr)
{
   brr.get_BinaryData(txHash_, 32);
   txOutIndex_ = brr.get_uint32_t();
}


void OutPoint::unserialize(BinaryData const & bd) 
{ 
   unserialize(bd.getPtr()); 
}
void OutPoint::unserialize(BinaryDataRef const & bdRef) 
{ 
   unserialize(bdRef.getPtr());
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// TxIn methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

TxIn::TxIn(void) :
   outPoint_(),
   scriptSize_(0),
   binScript_(0),
   sequence_(0),
   scriptType_(TXIN_SCRIPT_UNKNOWN)
{
   // Nothing to put here
}

TxIn::TxIn(OutPoint const & op,
     BinaryData const & script,
     uint32_t seq,
     bool coinbase) : 
   outPoint_(op),
   binScript_(script),
   sequence_(seq),
   scriptType_(TXIN_SCRIPT_UNKNOWN)
{
   scriptSize_ = (uint32_t)(binScript_.getSize());
}


void TxIn::serialize(BinaryWriter & bw)
{
   outPoint_.serialize(bw);
   bw.put_var_int(scriptSize_);
   bw.put_BinaryData(binScript_);
   bw.put_uint32_t(sequence_);
}

BinaryData TxIn::serialize(void)
{
   BinaryWriter bw(250);
   serialize(bw);
   return bw.getData();
}

void TxIn::unserialize(uint8_t const * ptr)
{
   outPoint_.unserialize(ptr);
   uint32_t viLen;
   scriptSize_ = (uint32_t)BtcUtils::readVarInt(ptr+36, &viLen);
   binScript_.copyFrom(ptr+36+viLen, scriptSize_);
   sequence_ = *(uint32_t*)(ptr+36+viLen+scriptSize_);

   // Derived values
   nBytes_ = 36+viLen+scriptSize_+4;
   scriptType_ = BtcUtils::getTxInScriptType(binScript_, outPoint_.getTxHash().getRef());
}
void TxIn::unserialize(BinaryReader & br)
{
   uint32_t posStart = br.getPosition();
   outPoint_.unserialize(br);
   scriptSize_ = (uint32_t)br.get_var_int();
   br.get_BinaryData(binScript_, scriptSize_);
   sequence_ = br.get_uint32_t();

   nBytes_ = br.getPosition() - posStart;
   scriptType_ = BtcUtils::getTxInScriptType(binScript_, outPoint_.getTxHash().getRef());
}
void TxIn::unserialize(BinaryRefReader & brr)
{
   uint32_t posStart = brr.getPosition();
   outPoint_.unserialize(brr);
   scriptSize_ = (uint32_t)brr.get_var_int();
   brr.get_BinaryData(binScript_, scriptSize_);
   sequence_ = brr.get_uint32_t();
   nBytes_ = brr.getPosition() - posStart;
   scriptType_ = BtcUtils::getTxInScriptType(binScript_, outPoint_.getTxHashRef());
}

void TxIn::unserialize(BinaryData const & str)
{
   unserialize(str.getPtr());
}
void TxIn::unserialize(BinaryDataRef const & str)
{
   unserialize(str.getPtr());
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// TxOut methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

TxOut::TxOut(void) :
   value_(0),
   scriptSize_(0),
   pkScript_(0),
   scriptType_(TXOUT_SCRIPT_UNKNOWN),
   recipientBinAddr20_(0),
   isMine_(false),
   isSpent_(false)

{
   // Nothing to put here
}

/////////////////////////////////////////////////////////////////////////////
TxOut::TxOut(uint64_t val, BinaryData const & scr) :
   value_(val),
   pkScript_(scr)
{
   scriptSize_ = (uint32_t)(pkScript_.getSize());
}


/////////////////////////////////////////////////////////////////////////////
void TxOut::serialize(BinaryWriter & bw)
{
   bw.put_uint64_t(value_);
   bw.put_var_int(scriptSize_);
   bw.put_BinaryData(pkScript_);
}

/////////////////////////////////////////////////////////////////////////////
BinaryData TxOut::serialize(void)
{
   BinaryWriter bw(45);
   serialize(bw);
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
void TxOut::unserialize(uint8_t const * ptr)
{
   value_ = *(uint64_t*)(ptr);
   uint32_t viLen;
   scriptSize_ = (uint32_t)BtcUtils::readVarInt(ptr+8, &viLen);
   pkScript_.copyFrom(ptr+8+viLen, scriptSize_);
   nBytes_ = 8 + viLen + scriptSize_;
   scriptType_ = BtcUtils::getTxOutScriptType(pkScript_.getRef());
   recipientBinAddr20_ = BtcUtils::getTxOutRecipientAddr(pkScript_, 
                                                         scriptType_);
}

/////////////////////////////////////////////////////////////////////////////
void TxOut::unserialize(BinaryReader & br)
{
   unserialize(br.getCurrPtr());
   br.advance(nBytes_);
}

/////////////////////////////////////////////////////////////////////////////
void TxOut::unserialize(BinaryRefReader & brr)
{
   unserialize(brr.getCurrPtr());
   brr.advance(nBytes_);
}


void TxOut::unserialize(BinaryData    const & str) 
{ 
   unserialize(str.getPtr());
}

void TxOut::unserialize(BinaryDataRef const & str) 
{ 
   unserialize(str.getPtr()); 
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Tx methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
Tx::Tx(void) :
   version_(0),
   numTxIn_(0),
   numTxOut_(0),
   txInList_(0),
   txOutList_(0),
   lockTime_(UINT32_MAX),
   thisHash_(32),
   headerPtr_(NULL),
   headerRefPtr_(NULL)
{
   // Nothing to put here
}

  
/////////////////////////////////////////////////////////////////////////////
OutPoint Tx::createOutPoint(int txOutIndex)
{
   return OutPoint(thisHash_, txOutIndex);
}


/////////////////////////////////////////////////////////////////////////////
void Tx::serialize(BinaryWriter & bw)
{
   bw.put_uint32_t(version_);
   bw.put_var_int(numTxIn_);
   for(uint32_t i=0; i<numTxIn_; i++)
   {
      txInList_[i].serialize(bw);
   }
   bw.put_var_int(numTxOut_);
   for(uint32_t i=0; i<numTxOut_; i++)
   {
      txOutList_[i].serialize(bw);
   }
   bw.put_uint32_t(lockTime_);
}

/////////////////////////////////////////////////////////////////////////////
BinaryData Tx::serialize(void)
{
   BinaryWriter bw(300);
   serialize(bw);
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
void Tx::unserialize(uint8_t const * ptr)
{
   // This would be too annoying to write in raw pointer-arithmatic
   // So I will just create a BinaryRefReader
   BinaryRefReader brr(ptr);
   unserialize(brr);
}

/////////////////////////////////////////////////////////////////////////////
void Tx::unserialize(BinaryReader & br)
{
   uint32_t posStart = br.getPosition();
   version_ = br.get_uint32_t();

   numTxIn_ = (uint32_t)br.get_var_int();
   txInList_ = vector<TxIn>(numTxIn_);
   for(uint32_t i=0; i<numTxIn_; i++)
      txInList_[i].unserialize(br); 

   numTxOut_ = (uint32_t)br.get_var_int();
   txOutList_ = vector<TxOut>(numTxOut_);
   for(uint32_t i=0; i<numTxOut_; i++)
      txOutList_[i].unserialize(br); 

   lockTime_ = br.get_uint32_t();
   nBytes_ = br.getPosition() - posStart;
}

/////////////////////////////////////////////////////////////////////////////
void Tx::unserialize(BinaryRefReader & brr)
{
   uint32_t posStart = brr.getPosition();
   version_ = brr.get_uint32_t();

   numTxIn_ = (uint32_t)brr.get_var_int();
   txInList_ = vector<TxIn>(numTxIn_);
   for(uint32_t i=0; i<numTxIn_; i++)
      txInList_[i].unserialize(brr); 

   numTxOut_ = (uint32_t)brr.get_var_int();
   txOutList_ = vector<TxOut>(numTxOut_);
   for(uint32_t i=0; i<numTxOut_; i++)
      txOutList_[i].unserialize(brr); 

   lockTime_ = brr.get_uint32_t();
   nBytes_ = brr.getPosition() - posStart;
}

/////////////////////////////////////////////////////////////////////////////
void Tx::unserialize(BinaryData const & str)
{
   unserialize(str.getPtr());
}
/////////////////////////////////////////////////////////////////////////////
void Tx::unserialize(BinaryDataRef const & str)
{
   unserialize(str.getPtr());
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// UnspentTxOut Methods
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
UnspentTxOut::UnspentTxOut(void) :
   txHash_(BtcUtils::EmptyHash_),
   txOutIndex_(0),
   txHeight_(0),
   value_(0),
   script_(BinaryData(0)),
   numConfirm_(0)
{
   // Nothing to do here
}

////////////////////////////////////////////////////////////////////////////////
void UnspentTxOut::init(TxOutRef & txout, uint32_t blkNum)
{
   txHash_     = txout.getParentTxPtr()->getThisHash();
   txOutIndex_ = txout.getIndex();
   txHeight_   = txout.getParentTxPtr()->getBlockHeight();
   value_      = txout.getValue();
   script_     = txout.getScript();
   updateNumConfirm(blkNum);
}

////////////////////////////////////////////////////////////////////////////////
BinaryData UnspentTxOut::getRecipientAddr(void) const
{
   return BtcUtils::getTxOutRecipientAddr(getScript());
}


////////////////////////////////////////////////////////////////////////////////
uint32_t UnspentTxOut::updateNumConfirm(uint32_t currBlkNum)
{
   if(txHeight_ == UINT32_MAX)
      numConfirm_ = 0;
   else
      numConfirm_ = currBlkNum - txHeight_ + 1;
   return numConfirm_;
}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareNaive(UnspentTxOut const & uto1, 
                                UnspentTxOut const & uto2)
{
   float val1 = uto1.getValue();
   float val2 = uto2.getValue();
   return (val1*uto1.numConfirm_ < val2*uto2.numConfirm_);
}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareTech1(UnspentTxOut const & uto1,
                                UnspentTxOut const & uto2)
{
   float val1 = pow((float)uto1.getValue(), 1.0f/3.0f);
   float val2 = pow((float)uto2.getValue(), 1.0f/3.0f);
   return (val1*uto1.numConfirm_ < val2*uto2.numConfirm_);

}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareTech2(UnspentTxOut const & uto1,
                                UnspentTxOut const & uto2)
{
   float val1 = pow(log10((float)uto1.getValue()) + 5, 5);
   float val2 = pow(log10((float)uto2.getValue()) + 5, 5);
   return (val1*uto1.numConfirm_ < val2*uto2.numConfirm_);

}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareTech3(UnspentTxOut const & uto1,
                                UnspentTxOut const & uto2)
{
   float val1 = pow(log10((float)uto1.getValue()) + 5, 4);
   float val2 = pow(log10((float)uto2.getValue()) + 5, 4);
   return (val1*uto1.numConfirm_ < val2*uto2.numConfirm_);
}


////////////////////////////////////////////////////////////////////////////////
void UnspentTxOut::sortTxOutVect(vector<UnspentTxOut> & utovect, int sortType)
{
   switch(sortType)
   {
   case 0: sort(utovect.begin(), utovect.end(), CompareNaive); break;
   case 1: sort(utovect.begin(), utovect.end(), CompareTech1); break;
   case 2: sort(utovect.begin(), utovect.end(), CompareTech2); break;
   case 3: sort(utovect.begin(), utovect.end(), CompareTech3); break;
   default: break; // do nothing
   }
}

















